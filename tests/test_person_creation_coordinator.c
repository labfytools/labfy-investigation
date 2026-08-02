#include "core/evidence_staging.h"
#include "core/person_creation_coordinator.h"
#include "database/database.h"
#include "database/statement.h"
#include "database/transaction.h"
#include "dao/identity_ocr_dao.h"
#include "dao/entity_dao.h"
#include <glib.h>
#include <glib/gstdio.h>

static guint64 count(Database *database, const char *table)
{
    char *sql = g_strdup_printf("SELECT COUNT(*) FROM %s;", table);
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    int64_t value = 0;
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_int64(statement, 0, &value));
    database_statement_finalize(statement);
    g_free(sql);
    return (guint64) value;
}
static void execute_sql(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(statement);
}
static void remove_tree(const char *root)
{
    GDir *directory = g_dir_open(root, 0, NULL);
    const char *name;
    if (directory == NULL) return;
    while ((name = g_dir_read_name(directory)) != NULL) {
        char *path = g_build_filename(root, name, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) remove_tree(path);
        else g_unlink(path);
        g_free(path);
    }
    g_dir_close(directory);
    g_rmdir(root);
}
typedef struct {
    guint calls;
} SessionCounter;
static gboolean session_changes_before_commit(gpointer data)
{
    SessionCounter *counter = data;
    return counter->calls++ == 0;
}
static void assert_empty_after_reopen(const char *database_path)
{
    static const char *const tables[] = {
        "entites", "preuves", "preuve_entites", "preuve_entite_sources",
        "person_role_assignments", "identity_ocr_runs",
        "identity_document_observations", "identity_field_observations",
        "person_evidence_factual_relations", "person_profile_fields",
        "person_ocr_field_projections"
    };
    Database *database = database_open(database_path);
    g_assert_nonnull(database);
    for (guint i = 0; i < G_N_ELEMENTS(tables); i++)
        g_assert_cmpuint(count(database, tables[i]), ==, 0);
    database_close(database);
}
static void test_failure_matrix(void)
{
    static const struct {
        PersonCreationFailurePoint point;
        guint occurrence;
    } cases[] = {
        {PERSON_CREATION_FAILURE_VALIDATE,0},
        {PERSON_CREATION_FAILURE_SESSION_BEFORE_START,0},
        {PERSON_CREATION_FAILURE_SOURCE_HASH,0},
        {PERSON_CREATION_FAILURE_CREATE_OCR_DIRECTORY,0},
        {PERSON_CREATION_FAILURE_COPY_OCR_TEXT,0},
        {PERSON_CREATION_FAILURE_HASH_OCR_TEXT,0},
        {PERSON_CREATION_FAILURE_COPY_OCR_TSV,0},
        {PERSON_CREATION_FAILURE_HASH_OCR_TSV,0},
        {PERSON_CREATION_FAILURE_CREATE_PERSON,0},
        {PERSON_CREATION_FAILURE_CREATE_ROLE,0},
        {PERSON_CREATION_FAILURE_CREATE_ROLE,1},
        {PERSON_CREATION_FAILURE_CREATE_ROLE,2},
        {PERSON_CREATION_FAILURE_IMPORT_EVIDENCE,0},
        {PERSON_CREATION_FAILURE_IMPORT_EVIDENCE,1},
        {PERSON_CREATION_FAILURE_IMPORT_EVIDENCE,2},
        {PERSON_CREATION_FAILURE_LINK_EVIDENCE,0},
        {PERSON_CREATION_FAILURE_LINK_EVIDENCE,1},
        {PERSON_CREATION_FAILURE_LINK_EVIDENCE,2},
        {PERSON_CREATION_FAILURE_INSERT_OCR_RUN,0},
        {PERSON_CREATION_FAILURE_INSERT_OCR_RUN,1},
        {PERSON_CREATION_FAILURE_INSERT_OCR_RUN,2},
        {PERSON_CREATION_FAILURE_INSERT_DOCUMENT_OBSERVATION,0},
        {PERSON_CREATION_FAILURE_INSERT_DOCUMENT_OBSERVATION,1},
        {PERSON_CREATION_FAILURE_INSERT_DOCUMENT_OBSERVATION,2},
        {PERSON_CREATION_FAILURE_INSERT_FIELD,0},
        {PERSON_CREATION_FAILURE_INSERT_FIELD,1},
        {PERSON_CREATION_FAILURE_INSERT_FIELD,2},
        {PERSON_CREATION_FAILURE_CREATE_SOURCE,0},
        {PERSON_CREATION_FAILURE_CREATE_SOURCE,1},
        {PERSON_CREATION_FAILURE_CREATE_SOURCE,2},
        {PERSON_CREATION_FAILURE_INSERT_FACTUAL_RELATION,0},
        {PERSON_CREATION_FAILURE_APPLY_OCR_PROJECTION,0},
        {PERSON_CREATION_FAILURE_SESSION_BEFORE_COMMIT,0},
        {PERSON_CREATION_FAILURE_ARTIFACT_TEXT_CHANGED,0},
        {PERSON_CREATION_FAILURE_ARTIFACT_TSV_CHANGED,0},
        {PERSON_CREATION_FAILURE_COMMIT,0},
        {PERSON_CREATION_FAILURE_COMMIT,0}
    };
    for (guint scenario = 0; scenario < G_N_ELEMENTS(cases); scenario++) {
        char *root = g_dir_make_tmp("labfy-coordinator-matrix-XXXXXX", NULL);
        char *database_directory = g_build_filename(
            root, "00_BaseDeDonnees", NULL);
        char *database_path = g_build_filename(
            database_directory, "Enquete.sqlite", NULL);
        g_assert_cmpint(g_mkdir_with_parents(database_directory,0700),==,0);
        g_assert_true(database_initialize(database_path,
            "Enquête SPECIMEN matrice", root));
        EvidenceStaging *staging = evidence_staging_new(NULL);
        PersonEvidenceSelection *selection = person_evidence_selection_new();
        GPtrArray *prepared = g_ptr_array_new_with_free_func(
            (GDestroyNotify) evidence_staging_result_free);
        GPtrArray *runs = g_ptr_array_new_with_free_func(
            (GDestroyNotify) identity_ocr_run_free);
        GPtrArray *roles = g_ptr_array_new_with_free_func(
            (GDestroyNotify) person_role_assignment_input_free);
        for (guint i = 0; i < 3; i++) {
            char *name = g_strdup_printf("SPECIMEN-%u.txt", i);
            char *source = g_build_filename(root, name, NULL);
            char *contents = g_strdup_printf("SPECIMEN SOURCE %u", i);
            g_assert_true(g_file_set_contents(source, contents, -1, NULL));
            EvidenceStagingResult *item = evidence_staging_prepare(
                staging, source, NULL, NULL);
            g_assert_nonnull(item);
            g_ptr_array_add(prepared, item);
            g_assert_true(person_evidence_selection_add_staged(selection,
                item->source_path, item->staging_path, item->original_name,
                item->mime_type, "text", item->size_bytes, item->sha256,
                NULL, item->prepared_at, NULL));
            const PersonEvidenceSelectionItem *selected =
                person_evidence_selection_get(selection, i);
            IdentityOcrRun *run = identity_ocr_run_new(
                person_evidence_selection_item_get_identifier(selected),
                item->sha256, "identity_card", "front", i + 1,
                "eng", "none");
            identity_ocr_run_set_outputs(run, "SPECIMEN", "eng", "params",
                "NOM: SPECIMEN\nPRÉNOM: TEST\nNUMÉRO: ABC123",
                "level\tpage_num\tblock_num\tpar_num\tline_num\tword_num\t"
                "left\ttop\twidth\theight\tconf\ttext\n");
            IdentityFieldObservation *field =
                identity_field_observation_new("surname", "SPECIMEN",
                    90, NULL, i);
            identity_field_observation_accept(field);
            identity_field_observation_confirm(field,"SPECIMEN");
            identity_ocr_run_add_field(run, field);
            g_ptr_array_add(runs, run);
            PersonRoleAssignmentInput role = {
                .role_code = i == 0 ? "victim" :
                    i == 1 ? "witness" : "mentioned_person",
                .provenance_kind = "manual"
            };
            g_ptr_array_add(roles,
                person_role_assignment_input_copy(&role));
            g_free(name); g_free(source); g_free(contents);
        }
        PersonEntityInput person = {
            .designation = "Personne SPECIMEN matrice",
            .identification_status = "suspected",
            .confidence = 10,
            .role_assignments = roles
        };
        PersonCreationFactualRelationInput factual_relation = {
            .evidence_selection_identifier =
                person_evidence_selection_item_get_identifier(
                    person_evidence_selection_get(selection, 0)),
            .ocr_run_identifier = identity_ocr_run_get_identifier(
                g_ptr_array_index(runs, 0)),
            .relation_type = "identity_observed_in",
            .factual_note = "Choix humain SPECIMEN"
        };
        GPtrArray *factual_relations = g_ptr_array_new();
        g_ptr_array_add(factual_relations, &factual_relation);
        IdentityOcrRun *projection_run=g_ptr_array_index(runs,0);
        IdentityFieldObservation *projection_field=g_ptr_array_index(
            (GPtrArray*)identity_ocr_run_get_fields(projection_run),0);
        PersonOcrFieldProjection *projection=person_ocr_field_projection_new(
            identity_ocr_run_get_evidence_id(projection_run),
            identity_ocr_run_get_identifier(projection_run),
            identity_field_observation_get_identifier(projection_field),
            "surname","SPECIMEN","complete","accepted","surname",NULL,
            PERSON_OCR_FILL_EMPTY,TRUE);
        GPtrArray *projections=g_ptr_array_new();g_ptr_array_add(projections,projection);
        PersonCreationCoordinatorOptions options = {
            .failure_point = cases[scenario].point,
            .failure_occurrence = cases[scenario].occurrence,
            .inject_compensation_failure =
                scenario == G_N_ELEMENTS(cases) - 1,
            .factual_relations = factual_relations
            ,.ocr_projections = projections
        };
        char *ocr_parent=g_build_filename(root,"02_Preuves_Traitees",
            "OCR",NULL);
        gboolean parent_preexisted=scenario%2==0;
        char *parent_marker=g_build_filename(ocr_parent,"SPECIMEN.marker",NULL);
        if(parent_preexisted){
            g_assert_cmpint(g_mkdir_with_parents(ocr_parent,0700),==,0);
            g_assert_true(g_file_set_contents(parent_marker,"SPECIMEN",-1,NULL));
        }
        Database *database = database_open(database_path);
        GError *error = NULL;
        PersonCreationCoordinatorResult *result =
            person_creation_coordinator_execute_with_options(database, root,
                &person, selection, runs, &options, NULL, &error);
        g_assert_null(result);
        g_assert_nonnull(error);
        if (options.inject_compensation_failure)
            g_assert_nonnull(strstr(error->message,
                "Compensation incomplète"));
        g_clear_error(&error);
        database_close(database);
        assert_empty_after_reopen(database_path);
        guint residual_runs=0;
        for(guint i=0;i<runs->len;i++){
            IdentityOcrRun *run=g_ptr_array_index(runs,i);
            char *run_directory=g_build_filename(ocr_parent,
                identity_ocr_run_get_identifier(run),NULL);
            if(g_file_test(run_directory,G_FILE_TEST_EXISTS))
                residual_runs++;
            if(!options.inject_compensation_failure&&
                g_file_test(run_directory,G_FILE_TEST_EXISTS))
                g_error("Résidu au scénario %u, point %u occurrence %u, "
                    "run %u",scenario,(guint)cases[scenario].point,
                    cases[scenario].occurrence,i);
            g_free(run_directory);
        }
        g_assert_cmpuint(residual_runs,==,
            options.inject_compensation_failure?1:0);
        if(parent_preexisted){
            g_assert_true(g_file_test(parent_marker,G_FILE_TEST_IS_REGULAR));
        }else if(!options.inject_compensation_failure)
            g_assert_false(g_file_test(ocr_parent,G_FILE_TEST_EXISTS));
        for (guint i = 0; i < prepared->len; i++) {
            EvidenceStagingResult *item = g_ptr_array_index(prepared, i);
            char *contents = NULL;
            g_assert_true(g_file_get_contents(item->source_path,
                &contents, NULL, NULL));
            g_assert_true(g_str_has_prefix(contents, "SPECIMEN SOURCE"));
            g_free(contents);
        }
        g_ptr_array_unref(roles);
        g_ptr_array_unref(factual_relations);
        g_ptr_array_unref(projections);person_ocr_field_projection_free(projection);
        g_ptr_array_unref(runs);
        person_evidence_selection_free(selection);
        g_ptr_array_unref(prepared);
        evidence_staging_free(staging);
        g_free(ocr_parent);g_free(parent_marker);
        g_free(database_directory); g_free(database_path);
        remove_tree(root); g_free(root);
    }
}
static void test_real_session_change_before_commit(void)
{
    char *root = g_dir_make_tmp("labfy-session-before-commit-XXXXXX", NULL);
    char *database_directory = g_build_filename(root,"00_BaseDeDonnees",NULL);
    char *database_path = g_build_filename(database_directory,
        "Enquete.sqlite", NULL);
    char *source = g_build_filename(root, "SPECIMEN.txt", NULL);
    g_assert_cmpint(g_mkdir_with_parents(database_directory,0700),==,0);
    g_assert_true(database_initialize(database_path,"SPECIMEN",root));
    g_assert_true(g_file_set_contents(source,"SPECIMEN",-1,NULL));
    EvidenceStaging *staging = evidence_staging_new(NULL);
    EvidenceStagingResult *prepared =
        evidence_staging_prepare(staging,source,NULL,NULL);
    PersonEvidenceSelection *selection = person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(selection,
        prepared->source_path,prepared->staging_path,prepared->original_name,
        prepared->mime_type,"text",prepared->size_bytes,prepared->sha256,
        NULL,prepared->prepared_at,NULL));
    PersonEntityInput person={.designation="SPECIMEN",
        .identification_status="unknown"};
    SessionCounter counter={0};
    PersonCreationCoordinatorOptions options={
        .session_check=session_changes_before_commit,
        .session_check_data=&counter};
    Database *database=database_open(database_path);
    GError *error=NULL;
    g_assert_null(person_creation_coordinator_execute_with_options(database,
        root,&person,selection,NULL,&options,NULL,&error));
    g_assert_nonnull(error);g_clear_error(&error);database_close(database);
    assert_empty_after_reopen(database_path);
    person_evidence_selection_free(selection);
    evidence_staging_result_free(prepared);evidence_staging_free(staging);
    g_free(database_directory);g_free(database_path);g_free(source);
    remove_tree(root);g_free(root);
}
static void test_existing_person_session_change_compensates(void)
{
    char *root=g_dir_make_tmp("labfy-existing-session-XXXXXX",NULL);
    char *database_directory=g_build_filename(root,"00_BaseDeDonnees",NULL);
    char *database_path=g_build_filename(database_directory,"Enquete.sqlite",NULL);
    char *source=g_build_filename(root,"SPECIMEN-IDENTITE.txt",NULL);
    g_assert_cmpint(g_mkdir_with_parents(database_directory,0700),==,0);
    g_assert_true(database_initialize(database_path,"SPECIMEN",root));
    g_assert_true(g_file_set_contents(source,"SPECIMEN IDENTITE",-1,NULL));
    Database *database=database_open(database_path);GError *error=NULL;
    EntityDao *entity_dao=entity_dao_new(database,&error);
    EntityRecord *person=entity_record_new(
        "22222222-2222-4222-8222-222222222222","person",
        "PERSONNE SPECIMEN","PERSONNE SPECIMEN",NULL,0,
        "2026-01-01T00:00:00Z","2026-01-01T00:00:00Z",
        ENTITY_STATUS_ACTIVE,&error);
    g_assert_true(entity_dao_insert(entity_dao,person,&error));
    entity_record_free(person);entity_dao_free(entity_dao);
    EvidenceStaging *staging=evidence_staging_new(&error);
    EvidenceStagingResult *prepared=evidence_staging_prepare(
        staging,source,NULL,&error);
    PersonEvidenceSelection *selection=person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(selection,
        prepared->source_path,prepared->staging_path,prepared->original_name,
        prepared->mime_type,"document",prepared->size_bytes,prepared->sha256,
        "SPECIMEN",prepared->prepared_at,&error));
    const PersonEvidenceSelectionItem *item=
        person_evidence_selection_get(selection,0);
    IdentityOcrRun *run=identity_ocr_run_new(
        person_evidence_selection_item_get_identifier(item),prepared->sha256,
        "identity_card","front",1,"fra","none");
    identity_ocr_run_set_outputs(run,"SPECIMEN","fra","SPECIMEN",
        "NOM SPECIMEN","TSV SPECIMEN");
    IdentityFieldObservation *field=identity_field_observation_new(
        "surname","SPECIMEN",99,NULL,0);
    identity_field_observation_accept(field);
    identity_ocr_run_add_field(run,field);
    GPtrArray *runs=g_ptr_array_new_with_free_func(
        (GDestroyNotify)identity_ocr_run_free);
    g_ptr_array_add(runs,run);
    SessionCounter counter={0};
    PersonCreationCoordinatorOptions options={
        .session_check=session_changes_before_commit,
        .session_check_data=&counter};
    g_assert_null(person_creation_coordinator_attach_to_existing_person(
        database,root,"22222222-2222-4222-8222-222222222222",
        selection,runs,NULL,&options,NULL,&error));
    g_assert_nonnull(error);g_clear_error(&error);
    g_assert_cmpuint(count(database,"entites"),==,1);
    g_assert_cmpuint(count(database,"preuves"),==,0);
    g_assert_cmpuint(count(database,"preuve_entites"),==,0);
    g_assert_cmpuint(count(database,"identity_ocr_runs"),==,0);
    char *ocr_root=g_build_filename(root,"02_Preuves_Traitees","OCR",NULL);
    g_assert_false(g_file_test(ocr_root,G_FILE_TEST_EXISTS));
    g_free(ocr_root);g_ptr_array_unref(runs);
    person_evidence_selection_free(selection);
    evidence_staging_result_free(prepared);evidence_staging_free(staging);
    database_close(database);g_free(source);g_free(database_directory);
    g_free(database_path);remove_tree(root);g_free(root);
}
static void test_success_and_rollback(void)
{
    char *root = g_dir_make_tmp("labfy-coordinator-XXXXXX", NULL);
    char *database_directory =
        g_build_filename(root, "00_BaseDeDonnees", NULL);
    char *database_path =
        g_build_filename(database_directory, "Enquete.sqlite", NULL);
    char *source_one = g_build_filename(root, "SPECIMEN-one.txt", NULL);
    char *source_two = g_build_filename(root, "SPECIMEN-two.txt", NULL);
    EvidenceStaging *staging = NULL;
    EvidenceStagingResult *prepared_one = NULL, *prepared_two = NULL;
    PersonEvidenceSelection *selection = NULL;
    PersonCreationCoordinatorResult *result = NULL;
    Database *database;
    PersonEntityInput person = {
        .designation = "Personne SPECIMEN",
        .identification_status = "suspected",
        .confidence = 20
    };
    GError *error = NULL;
    g_assert_cmpint(g_mkdir_with_parents(database_directory, 0700), ==, 0);
    g_assert_true(database_initialize(
        database_path, "Enquête SPECIMEN", root));
    g_assert_true(g_file_set_contents(
        source_one, "SPECIMEN-ONE", -1, NULL));
    g_assert_true(g_file_set_contents(
        source_two, "SPECIMEN-TWO", -1, NULL));
    staging = evidence_staging_new(&error);
    prepared_one = evidence_staging_prepare(
        staging, source_one, NULL, &error);
    prepared_two = evidence_staging_prepare(
        staging, source_two, NULL, &error);
    g_assert_no_error(error);
    selection = person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(selection,
        prepared_one->source_path, prepared_one->staging_path,
        prepared_one->original_name, prepared_one->mime_type, "text",
        prepared_one->size_bytes, prepared_one->sha256, NULL,
        prepared_one->prepared_at, &error));
    GPtrArray *coordinator_runs = g_ptr_array_new_with_free_func(
        (GDestroyNotify) identity_ocr_run_free);
    IdentityOcrRun *coordinator_run = identity_ocr_run_new(
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(selection, 0)),
        prepared_one->sha256, "identity_card", "front", 1, "eng", "none");
    identity_ocr_run_set_outputs(coordinator_run, "tesseract SPECIMEN", "eng",
        "paramètres SPECIMEN", "NOM SPECIMEN", "TSV SPECIMEN");
    IdentityFieldObservation *coordinator_field =
        identity_field_observation_new("surname", "SPECIMEN", 90, NULL, 0);
    identity_field_observation_accept(coordinator_field);
    identity_ocr_run_add_field(coordinator_run, coordinator_field);
    IdentityFieldObservation *manual_field =
        identity_field_observation_new_manual(
            "nationality", "FRANÇAISE", 1);
    g_assert_nonnull(manual_field);
    g_assert_true(identity_field_observation_modify(manual_field,
        "FRANÇAISE", "saisie manuelle contrôlée"));
    identity_ocr_run_add_field(coordinator_run, manual_field);
    identity_ocr_run_set_factual_notes(coordinator_run,
        "Document tronqué ; bord gauche coupé. "
        "Fragment visible : « publique française ».");
    g_ptr_array_add(coordinator_runs, coordinator_run);
    PersonEvidenceSelection *task_selection =
        person_evidence_selection_copy(selection);
    PersonCreationFactualRelationInput explicit_relation = {
        .evidence_selection_identifier =
            person_evidence_selection_item_get_identifier(
                person_evidence_selection_get(task_selection, 0)),
        .ocr_run_identifier =
            identity_ocr_run_get_identifier(coordinator_run),
        .relation_type = "data_extracted_from",
        .factual_note = "Choix humain explicite SPECIMEN"
    };
    GPtrArray *explicit_relations = g_ptr_array_new();
    g_ptr_array_add(explicit_relations, &explicit_relation);
    PersonCreationCoordinatorOptions explicit_options = {
        .factual_relations = explicit_relations
    };
    database = database_open(database_path);
    result = person_creation_coordinator_execute_with_options(
        database, root, &person, task_selection, coordinator_runs,
        &explicit_options, NULL, &error);
    g_ptr_array_unref(explicit_relations);
    person_evidence_selection_free(task_selection);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpuint(result->evidence_identifiers->len, ==, 1);
    g_assert_cmpuint(count(database, "entites"), ==, 1);
    g_assert_cmpuint(count(database, "preuves"), ==, 1);
    g_assert_cmpuint(count(database, "preuve_entites"), ==, 1);
    g_assert_cmpuint(count(database, "preuve_entite_sources"), ==, 1);
    g_assert_cmpuint(count(database, "identity_ocr_runs"), ==, 1);
    g_assert_cmpuint(count(database, "identity_document_observations"), ==, 1);
    g_assert_cmpuint(count(database, "identity_field_observations"), ==, 2);
    g_assert_cmpuint(count(database,
        "person_evidence_factual_relations"), ==, 1);
    char *ocr_root = g_build_filename(root, "02_Preuves_Traitees", "OCR",
        identity_ocr_run_get_identifier(coordinator_run), NULL);
    char *ocr_text = g_build_filename(ocr_root, "ocr.txt", NULL);
    char *ocr_tsv = g_build_filename(ocr_root, "ocr.tsv", NULL);
    g_assert_true(g_file_test(ocr_text, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(ocr_tsv, G_FILE_TEST_IS_REGULAR));
    IdentityOcrRun *ocr = identity_ocr_run_new(
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(selection, 0)),
        prepared_one->sha256, "identity_card", "front", 1, "eng",
        "none");
    identity_ocr_run_set_outputs(ocr, "tesseract 5 SPECIMEN", "eng",
        "paramètres SPECIMEN", "SURNAME SPECIMEN", "TSV SPECIMEN");
    IdentitySourceBox box = {.page=1,.x=1,.y=2,.width=30,.height=10,
        .image_width=80,.image_height=40,.available=TRUE};
    IdentityFieldObservation *accepted =
        identity_field_observation_new("surname", "SPECIMEN", 91, &box, 0);
    IdentityFieldObservation *modified =
        identity_field_observation_new("given_names", "ALICE", 80, &box, 1);
    IdentityFieldObservation *rejected =
        identity_field_observation_new("birth_place", "TEST", 70, NULL, 2);
    IdentityFieldObservation *mrz =
        identity_field_observation_new("document_number", "MRZSPECIMEN", 99,
            &box, 3);
    IdentityFieldObservation *conflict =
        identity_field_observation_new("birth_date", "01/01/1990", 50,
            NULL, 4);
    identity_field_observation_accept(accepted);
    identity_field_observation_modify(modified, "ALICE SPECIMEN", "revue");
    identity_field_observation_reject(rejected);
    identity_field_observation_accept(mrz);
    g_assert_true(identity_field_observation_set_origin(mrz, "mrz"));
    identity_field_observation_mark_conflict(conflict);
    identity_ocr_run_add_field(ocr, accepted);
    identity_ocr_run_add_field(ocr, modified);
    identity_ocr_run_add_field(ocr, rejected);
    identity_ocr_run_add_field(ocr, mrz);
    identity_ocr_run_add_field(ocr, conflict);
    IdentityOcrDao *ocr_dao = identity_ocr_dao_new(database);
    const char *artifact_sha =
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    g_assert_true(database_transaction_begin(database));
    g_assert_true(identity_ocr_dao_insert(ocr_dao,
        result->person_identifier,
        g_ptr_array_index(result->evidence_identifiers, 0), ocr,
        "02_Preuves_Traitees/OCR/run/ocr.txt", artifact_sha,
        "02_Preuves_Traitees/OCR/run/ocr.tsv", artifact_sha,
        "2026-07-28T12:00:00Z", &error));
    g_assert_true(database_transaction_rollback(database));
    g_assert_cmpuint(count(database, "identity_ocr_runs"), ==, 1);
    g_assert_true(database_transaction_begin(database));
    g_assert_true(identity_ocr_dao_insert(ocr_dao,
        result->person_identifier,
        g_ptr_array_index(result->evidence_identifiers, 0), ocr,
        "02_Preuves_Traitees/OCR/run/ocr.txt", artifact_sha,
        "02_Preuves_Traitees/OCR/run/ocr.tsv", artifact_sha,
        "2026-07-28T12:00:00Z", &error));
    g_assert_true(database_transaction_commit(database));
    g_assert_cmpuint(count(database, "identity_ocr_runs"), ==, 2);
    g_assert_cmpuint(count(database, "identity_document_observations"), ==, 2);
    g_assert_cmpuint(count(database, "identity_field_observations"), ==, 7);
    GPtrArray *read_runs = identity_ocr_dao_list_runs_by_evidence(ocr_dao,
        g_ptr_array_index(result->evidence_identifiers, 0), &error);
    g_assert_no_error(error); g_assert_cmpuint(read_runs->len, ==, 2);
    IdentityOcrRunRecord *read_run = g_ptr_array_index(read_runs, 1);
    g_assert_cmpstr(read_run->engine, ==, "tesseract");
    g_assert_cmpstr(read_run->engine_version, ==, "tesseract 5 SPECIMEN");
    g_assert_cmpstr(read_run->requested_languages, ==, "eng");
    g_assert_cmpstr(read_run->preprocessing_profile, ==, "none");
    g_assert_null(read_run->error_message);
    IdentityOcrRunRecord *found_run = identity_ocr_dao_find_run(ocr_dao,
        read_run->id, &error);
    g_assert_no_error(error); g_assert_cmpstr(found_run->id, ==, read_run->id);
    identity_ocr_run_record_free(found_run);
    GPtrArray *documents = identity_ocr_dao_list_documents_by_person(ocr_dao,
        result->person_identifier, &error);
    g_assert_no_error(error); g_assert_cmpuint(documents->len, ==, 2);
    IdentityDocumentObservationRecord *document =
        g_ptr_array_index(documents, 1);
    g_assert_cmpstr(document->person_id, ==, result->person_identifier);
    g_assert_cmpstr(document->document_side, ==, "front");
    g_assert_null(document->issuing_country_declared);
    GPtrArray *read_fields = identity_ocr_dao_list_fields_by_document(ocr_dao,
        document->id, &error);
    g_assert_no_error(error); g_assert_cmpuint(read_fields->len, ==, 5);
    IdentityFieldObservationRecord *field0=g_ptr_array_index(read_fields,0);
    IdentityFieldObservationRecord *field1=g_ptr_array_index(read_fields,1);
    IdentityFieldObservationRecord *field2=g_ptr_array_index(read_fields,2);
    IdentityFieldObservationRecord *field3=g_ptr_array_index(read_fields,3);
    IdentityFieldObservationRecord *field4=g_ptr_array_index(read_fields,4);
    g_assert_cmpstr(field0->origin, ==, "ocr");
    g_assert_cmpstr(field1->origin, ==, "manual_override");
    g_assert_cmpstr(field1->raw_value, ==, "ALICE");
    g_assert_cmpstr(field1->corrected_value, ==, "ALICE SPECIMEN");
    g_assert_cmpstr(field2->review_status, ==, "rejected");
    g_assert_cmpstr(field3->origin, ==, "mrz");
    g_assert_cmpstr(field4->review_status, ==, "conflict");
    g_assert_true(field0->has_source_box);
    g_assert_null(identity_ocr_dao_find_field(ocr_dao,
        "00000000-0000-4000-8000-000000000000", &error));
    g_assert_no_error(error);
    g_ptr_array_unref(read_fields);g_ptr_array_unref(documents);
    g_ptr_array_unref(read_runs);
    identity_ocr_dao_free(ocr_dao);
    identity_ocr_run_free(ocr);
    char *persisted_person = g_strdup(result->person_identifier);
    char *persisted_evidence = g_strdup(
        g_ptr_array_index(result->evidence_identifiers, 0));
    person_creation_coordinator_result_free(result);
    person_evidence_selection_free(selection);
    selection = person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(selection,
        prepared_two->source_path, prepared_two->staging_path,
        prepared_two->original_name, prepared_two->mime_type, "text",
        prepared_two->size_bytes, prepared_two->sha256, NULL,
        prepared_two->prepared_at, &error));
    GPtrArray *failed_runs = g_ptr_array_new_with_free_func(
        (GDestroyNotify) identity_ocr_run_free);
    IdentityOcrRun *failed_run = identity_ocr_run_new(
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(selection, 0)),
        prepared_two->sha256, "identity_card", "back", 1, "eng", "none");
    identity_ocr_run_set_outputs(failed_run, "SPECIMEN", "eng", "params",
        "SPECIMEN", "TSV");
    g_ptr_array_add(failed_runs, failed_run);
    char *failed_directory = g_build_filename(root, "02_Preuves_Traitees",
        "OCR", identity_ocr_run_get_identifier(failed_run), NULL);
    execute_sql(database, "CREATE TEMP TRIGGER fail_identity_ocr "
        "BEFORE INSERT ON identity_ocr_runs BEGIN SELECT RAISE(ABORT,"
        "'SPECIMEN injected failure'); END;");
    result = person_creation_coordinator_execute(database, root, &person,
        selection, failed_runs, NULL, &error);
    g_assert_null(result); g_assert_nonnull(error); g_clear_error(&error);
    g_assert_false(g_file_test(failed_directory, G_FILE_TEST_EXISTS));
    g_assert_cmpuint(count(database, "entites"), ==, 1);
    execute_sql(database, "DROP TRIGGER fail_identity_ocr;");
    g_free(failed_directory); g_ptr_array_unref(failed_runs);
    person_evidence_selection_free(selection);
    selection = person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(selection,
        prepared_two->source_path, prepared_two->staging_path,
        prepared_two->original_name, prepared_two->mime_type, "missing_type",
        prepared_two->size_bytes, prepared_two->sha256, NULL,
        prepared_two->prepared_at, &error));
    result = person_creation_coordinator_execute(database, root, &person,
        selection, NULL, NULL, &error);
    g_assert_null(result);
    g_assert_nonnull(error);
    g_clear_error(&error);
    g_assert_cmpuint(count(database, "entites"), ==, 1);
    g_assert_cmpuint(count(database, "preuves"), ==, 1);
    database_close(database);
    database = database_open(database_path);
    g_assert_nonnull(database);
    ocr_dao = identity_ocr_dao_new(database);
    read_runs = identity_ocr_dao_list_runs_by_evidence(ocr_dao,
        persisted_evidence, &error);
    g_assert_no_error(error); g_assert_cmpuint(read_runs->len, ==, 2);
    documents = identity_ocr_dao_list_documents_by_person(ocr_dao,
        persisted_person, &error);
    g_assert_no_error(error); g_assert_cmpuint(documents->len, ==, 2);
    g_ptr_array_unref(read_runs); g_ptr_array_unref(documents);
    identity_ocr_dao_free(ocr_dao); database_close(database);
    g_free(persisted_person); g_free(persisted_evidence);
    g_ptr_array_unref(coordinator_runs);
    person_evidence_selection_free(selection);
    evidence_staging_result_free(prepared_one);
    evidence_staging_result_free(prepared_two);
    evidence_staging_free(staging);
    g_free(database_directory); g_free(database_path);
    g_free(source_one); g_free(source_two);
    g_free(ocr_root); g_free(ocr_text); g_free(ocr_tsv);
    remove_tree(root); g_free(root);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-coordinator/success-rollback",
        test_success_and_rollback);
    g_test_add_func("/person-coordinator/failure-matrix",
        test_failure_matrix);
    g_test_add_func("/person-coordinator/session-before-commit",
        test_real_session_change_before_commit);
    g_test_add_func("/person-coordinator/existing-session-compensation",
        test_existing_person_session_change_compensates);
    return g_test_run();
}
