/******************************************************************************
 * @file test_investigation_graph_load_task.c
 * @brief Tests du chargement asynchrone du graphe d'enquête.
 ******************************************************************************/

#include "core/investigation_graph_load_task.h"
#include "core/investigation_graph_load_task_test.h"

#include "dao/entity_dao.h"
#include "dao/relation_dao.h"

#include "database/database.h"

#include "models/entity_record.h"
#include "models/investigation_graph_model.h"
#include "models/relation_record.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#define TEST_ENTITY_A_IDENTIFIER \
    "10000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_B_IDENTIFIER \
    "20000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_C_IDENTIFIER \
    "30000000-0000-4000-8000-000000000001"

#define TEST_RELATION_AB_IDENTIFIER \
    "40000000-0000-4000-8000-000000000001"

#define TEST_RELATION_AC_IDENTIFIER \
    "40000000-0000-4000-8000-000000000002"

#define TEST_RELATION_CA_IDENTIFIER \
    "40000000-0000-4000-8000-000000000003"

/**
 * @brief Base SQLite temporaire utilisée par les tests.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;
    EntityDao *entity_dao;
    RelationDao *relation_dao;
} TestGraphLoadTaskFixture;

/**
 * @brief Résultats observés depuis le callback asynchrone.
 */
typedef struct
{
    GMainLoop *main_loop;
    GThread *main_thread;

    guint callback_count;
    guint destroy_count;

    gboolean timed_out;

    InvestigationGraphModel *graph_model;
    GError *error;
} TestGraphLoadTaskObservation;

/**
 * @brief Données possédées par une exécution asynchrone.
 */
typedef struct
{
    TestGraphLoadTaskObservation *observation;
} TestGraphLoadTaskCallbackData;

/**
 * @brief Vérifie le domaine et le code d'une erreur publique.
 */
static void test_graph_load_task_assert_error(
    const GError *error,
    InvestigationGraphLoadTaskError expected_code
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR
    );

    assert(
        error->code ==
        (gint) expected_code
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Supprime un fichier temporaire s'il existe.
 */
static void test_graph_load_task_remove_optional_file(
    const char *path
)
{
    if (path == NULL)
    {
        return;
    }

    if (g_file_test(
            path,
            G_FILE_TEST_EXISTS
        ))
    {
        assert(
            g_remove(
                path
            ) == 0
        );
    }
}

/**
 * @brief Crée une fixture SQLite réelle.
 */
static TestGraphLoadTaskFixture
test_graph_load_task_fixture_create(void)
{
    TestGraphLoadTaskFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-graph-load-task-test-XXXXXX",
            &error
        );

    assert(fixture.temporary_directory != NULL);
    assert(error == NULL);

    fixture.database_path =
        g_build_filename(
            fixture.temporary_directory,
            "Enquete.sqlite",
            NULL
        );

    assert(fixture.database_path != NULL);

    assert(
        database_initialize(
            fixture.database_path,
            "Enquete_GraphLoadTask",
            fixture.temporary_directory
        )
    );

    fixture.database =
        database_open(
            fixture.database_path
        );

    assert(fixture.database != NULL);

    fixture.entity_dao =
        entity_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.entity_dao != NULL);
    assert(error == NULL);

    fixture.relation_dao =
        relation_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.relation_dao != NULL);
    assert(error == NULL);

    return fixture;
}

/**
 * @brief Ferme les objets d'écriture avant le démarrage du worker.
 */
static void test_graph_load_task_fixture_close_database(
    TestGraphLoadTaskFixture *fixture
)
{
    assert(fixture != NULL);

    if (fixture->relation_dao != NULL)
    {
        relation_dao_free(
            fixture->relation_dao
        );

        fixture->relation_dao =
            NULL;
    }

    if (fixture->entity_dao != NULL)
    {
        entity_dao_free(
            fixture->entity_dao
        );

        fixture->entity_dao =
            NULL;
    }

    if (fixture->database != NULL)
    {
        database_close(
            fixture->database
        );

        fixture->database =
            NULL;
    }
}

/**
 * @brief Détruit la fixture et ses éventuels fichiers SQLite auxiliaires.
 */
static void test_graph_load_task_fixture_clear(
    TestGraphLoadTaskFixture *fixture
)
{
    char *write_ahead_log_path =
        NULL;

    char *shared_memory_path =
        NULL;

    char *journal_path =
        NULL;

    assert(fixture != NULL);

    test_graph_load_task_fixture_close_database(
        fixture
    );

    write_ahead_log_path =
        g_strconcat(
            fixture->database_path,
            "-wal",
            NULL
        );

    shared_memory_path =
        g_strconcat(
            fixture->database_path,
            "-shm",
            NULL
        );

    journal_path =
        g_strconcat(
            fixture->database_path,
            "-journal",
            NULL
        );

    test_graph_load_task_remove_optional_file(
        write_ahead_log_path
    );

    test_graph_load_task_remove_optional_file(
        shared_memory_path
    );

    test_graph_load_task_remove_optional_file(
        journal_path
    );

    test_graph_load_task_remove_optional_file(
        fixture->database_path
    );

    assert(
        g_rmdir(
            fixture->temporary_directory
        ) == 0
    );

    g_free(
        journal_path
    );

    g_free(
        shared_memory_path
    );

    g_free(
        write_ahead_log_path
    );

    g_free(
        fixture->database_path
    );

    g_free(
        fixture->temporary_directory
    );

    memset(
        fixture,
        0,
        sizeof(*fixture)
    );
}

/**
 * @brief Crée une entité valide.
 */
static EntityRecord *test_graph_load_task_create_entity(
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        entity_record_new(
            identifier,
            "person",
            value,
            NULL,
            NULL,
            80,
            "2026-07-21T22:00:00Z",
            "2026-07-21T22:00:00Z",
            ENTITY_STATUS_ACTIVE,
            &error
        );

    assert(entity_record != NULL);
    assert(error == NULL);

    return entity_record;
}

/**
 * @brief Insère une entité avec le DAO réel.
 */
static void test_graph_load_task_insert_entity(
    TestGraphLoadTaskFixture *fixture,
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    assert(fixture != NULL);
    assert(fixture->entity_dao != NULL);

    entity_record =
        test_graph_load_task_create_entity(
            identifier,
            value
        );

    assert(
        entity_dao_insert(
            fixture->entity_dao,
            entity_record,
            &error
        )
    );

    assert(error == NULL);

    entity_record_free(
        entity_record
    );
}

/**
 * @brief Crée une relation valide.
 */
static RelationRecord *test_graph_load_task_create_relation(
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_record_new(
            identifier,
            source_identifier,
            target_identifier,
            relation_type,
            NULL,
            "Relation asynchrone de test.",
            75,
            "2026-07-21T22:10:00Z",
            "2026-07-21T22:10:00Z",
            RELATION_STATUS_ACTIVE,
            &error
        );

    assert(relation_record != NULL);
    assert(error == NULL);

    return relation_record;
}

/**
 * @brief Insère une relation avec le DAO réel.
 */
static void test_graph_load_task_insert_relation(
    TestGraphLoadTaskFixture *fixture,
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    assert(fixture != NULL);
    assert(fixture->relation_dao != NULL);

    relation_record =
        test_graph_load_task_create_relation(
            identifier,
            source_identifier,
            target_identifier,
            relation_type
        );

    assert(
        relation_dao_insert(
            fixture->relation_dao,
            relation_record,
            &error
        )
    );

    assert(error == NULL);

    relation_record_free(
        relation_record
    );
}

/**
 * @brief Insère le graphe standard utilisé par plusieurs tests.
 */
static void test_graph_load_task_insert_standard_graph(
    TestGraphLoadTaskFixture *fixture
)
{
    test_graph_load_task_insert_entity(
        fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_graph_load_task_insert_entity(
        fixture,
        TEST_ENTITY_B_IDENTIFIER,
        "Personne B"
    );

    test_graph_load_task_insert_entity(
        fixture,
        TEST_ENTITY_C_IDENTIFIER,
        "Personne C"
    );

    test_graph_load_task_insert_relation(
        fixture,
        TEST_RELATION_AB_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "uses"
    );

    test_graph_load_task_insert_relation(
        fixture,
        TEST_RELATION_AC_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_C_IDENTIFIER,
        "controls"
    );

    test_graph_load_task_insert_relation(
        fixture,
        TEST_RELATION_CA_IDENTIFIER,
        TEST_ENTITY_C_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        "knows"
    );
}

/**
 * @brief Initialise une observation asynchrone.
 */
static TestGraphLoadTaskObservation
test_graph_load_task_observation_create(void)
{
    TestGraphLoadTaskObservation observation =
        {0};

    observation.main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    assert(observation.main_loop != NULL);

    observation.main_thread =
        g_thread_self();

    return observation;
}

/**
 * @brief Libère le résultat d'une observation.
 */
static void test_graph_load_task_observation_clear(
    TestGraphLoadTaskObservation *observation
)
{
    assert(observation != NULL);

    investigation_graph_model_free(
        observation->graph_model
    );

    g_clear_error(
        &observation->error
    );

    g_main_loop_unref(
        observation->main_loop
    );

    memset(
        observation,
        0,
        sizeof(*observation)
    );
}

/**
 * @brief Copie le résultat final reçu sur le thread principal.
 */
static void test_graph_load_task_callback(
    InvestigationGraphLoadTask *load_task,
    InvestigationGraphModel *graph_model,
    const GError *error,
    gpointer user_data
)
{
    TestGraphLoadTaskCallbackData *callback_data =
        user_data;

    TestGraphLoadTaskObservation *observation =
        NULL;

    assert(load_task != NULL);
    assert(callback_data != NULL);
    assert(callback_data->observation != NULL);

    observation =
        callback_data->observation;

    assert(
        g_thread_self() ==
        observation->main_thread
    );

    observation->callback_count++;

    assert(
        observation->callback_count ==
        1
    );

    assert(
        (graph_model != NULL && error == NULL) ||
        (graph_model == NULL && error != NULL)
    );

    observation->graph_model =
        graph_model;

    if (error != NULL)
    {
        observation->error =
            g_error_copy(
                error
            );
    }

    g_main_loop_quit(
        observation->main_loop
    );
}

/**
 * @brief Vérifie la destruction unique des données utilisateur.
 */
static void test_graph_load_task_callback_data_free(
    gpointer user_data
)
{
    TestGraphLoadTaskCallbackData *callback_data =
        user_data;

    assert(callback_data != NULL);
    assert(callback_data->observation != NULL);

    callback_data->observation->destroy_count++;

    g_free(
        callback_data
    );
}

/**
 * @brief Démarre une tâche avec une observation possédée.
 */
static void test_graph_load_task_start_or_abort(
    InvestigationGraphLoadTask *load_task,
    TestGraphLoadTaskObservation *observation
)
{
    TestGraphLoadTaskCallbackData *callback_data =
        NULL;

    GError *error =
        NULL;

    callback_data =
        g_new0(
            TestGraphLoadTaskCallbackData,
            1
        );

    callback_data->observation =
        observation;

    assert(
        investigation_graph_load_task_start(
            load_task,
            test_graph_load_task_callback,
            callback_data,
            test_graph_load_task_callback_data_free,
            &error
        )
    );

    assert(error == NULL);
}

/**
 * @brief Source de sécurité arrêtant une attente anormalement longue.
 */
static gboolean test_graph_load_task_timeout(
    gpointer user_data
)
{
    TestGraphLoadTaskObservation *observation =
        user_data;

    assert(observation != NULL);

    observation->timed_out =
        TRUE;

    g_main_loop_quit(
        observation->main_loop
    );

    return G_SOURCE_REMOVE;
}

/**
 * @brief Attend le callback final avec un timeout uniquement défensif.
 */
static void test_graph_load_task_wait_for_callback(
    TestGraphLoadTaskObservation *observation
)
{
    guint timeout_source =
        0;

    assert(observation != NULL);

    timeout_source =
        g_timeout_add_seconds(
            5,
            test_graph_load_task_timeout,
            observation
        );

    assert(timeout_source != 0);

    if (observation->callback_count == 0)
    {
        g_main_loop_run(
            observation->main_loop
        );
    }

    if (!observation->timed_out)
    {
        assert(
            g_source_remove(
                timeout_source
            )
        );
    }

    assert(!observation->timed_out);
    assert(observation->callback_count == 1);
    assert(observation->destroy_count == 1);
}

/**
 * @brief Exécute une source idle pour vérifier la réactivité du contexte.
 */
static gboolean test_graph_load_task_mark_idle(
    gpointer user_data
)
{
    gboolean *idle_executed =
        user_data;

    assert(idle_executed != NULL);

    *idle_executed =
        TRUE;

    return G_SOURCE_REMOVE;
}

/**
 * @brief Attend la finalisation interne d'une tâche détruite.
 */
static void test_graph_load_task_wait_for_internal_completion(void)
{
    gint64 deadline =
        g_get_monotonic_time() +
        (5 * G_TIME_SPAN_SECOND);

    while (investigation_graph_load_task_test_get_completion_count() == 0 &&
           g_get_monotonic_time() < deadline)
    {
        g_main_context_iteration(
            NULL,
            TRUE
        );
    }

    assert(
        investigation_graph_load_task_test_get_completion_count() ==
        1
    );
}

/**
 * @brief Vérifie la construction, les arguments et l'état initial.
 */
static void test_graph_load_task_lifecycle(void)
{
    InvestigationGraphLoadTask *load_task =
        NULL;

    GError *error =
        NULL;

    load_task =
        investigation_graph_load_task_new(
            NULL,
            &error
        );

    assert(load_task == NULL);

    test_graph_load_task_assert_error(
        error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    load_task =
        investigation_graph_load_task_new(
            "",
            NULL
        );

    assert(load_task == NULL);

    load_task =
        investigation_graph_load_task_new(
            "/tmp/labfy-unused.sqlite",
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    assert(
        !investigation_graph_load_task_is_running(
            load_task
        )
    );

    assert(
        !investigation_graph_load_task_is_running(
            NULL
        )
    );

    assert(
        !investigation_graph_load_task_start(
            load_task,
            NULL,
            NULL,
            NULL,
            &error
        )
    );

    test_graph_load_task_assert_error(
        error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    investigation_graph_load_task_cancel(
        NULL
    );

    investigation_graph_load_task_free(
        load_task
    );

    investigation_graph_load_task_free(
        NULL
    );
}

/**
 * @brief Vérifie une base vide, l'état actif et l'absence de blocage.
 */
static void test_graph_load_task_empty_database(void)
{
    TestGraphLoadTaskFixture fixture =
        test_graph_load_task_fixture_create();

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation observation =
        test_graph_load_task_observation_create();

    gboolean idle_executed =
        FALSE;

    GError *error =
        NULL;

    test_graph_load_task_fixture_close_database(
        &fixture
    );

    investigation_graph_load_task_test_reset();
    investigation_graph_load_task_test_pause_before_open();

    load_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &observation
    );

    investigation_graph_load_task_test_wait_until_before_open();

    assert(
        investigation_graph_load_task_is_running(
            load_task
        )
    );

    assert(
        g_idle_add(
            test_graph_load_task_mark_idle,
            &idle_executed
        ) != 0
    );

    while (!idle_executed)
    {
        g_main_context_iteration(
            NULL,
            TRUE
        );
    }

    assert(idle_executed);

    assert(
        !investigation_graph_load_task_start(
            load_task,
            test_graph_load_task_callback,
            NULL,
            NULL,
            &error
        )
    );

    test_graph_load_task_assert_error(
        error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_ALREADY_RUNNING
    );

    g_clear_error(
        &error
    );

    investigation_graph_load_task_test_release_before_open();

    test_graph_load_task_wait_for_callback(
        &observation
    );

    assert(observation.error == NULL);
    assert(observation.graph_model != NULL);

    assert(
        investigation_graph_model_get_entity_count(
            observation.graph_model
        ) == 0
    );

    assert(
        investigation_graph_model_get_relation_count(
            observation.graph_model
        ) == 0
    );

    assert(
        !investigation_graph_load_task_is_running(
            load_task
        )
    );

    investigation_graph_load_task_free(
        load_task
    );

    test_graph_load_task_observation_clear(
        &observation
    );

    test_graph_load_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement complet du graphe et de ses index.
 */
static void test_graph_load_task_complete_graph(void)
{
    TestGraphLoadTaskFixture fixture =
        test_graph_load_task_fixture_create();

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation observation =
        test_graph_load_task_observation_create();

    const RelationRecord *relation_ab =
        NULL;

    GPtrArray *outgoing_relations =
        NULL;

    GPtrArray *incoming_relations =
        NULL;

    GError *error =
        NULL;

    test_graph_load_task_insert_standard_graph(
        &fixture
    );

    test_graph_load_task_fixture_close_database(
        &fixture
    );

    investigation_graph_load_task_test_reset();

    load_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &observation
    );

    test_graph_load_task_wait_for_callback(
        &observation
    );

    assert(observation.error == NULL);
    assert(observation.graph_model != NULL);

    assert(
        investigation_graph_model_get_entity_count(
            observation.graph_model
        ) == 3
    );

    assert(
        investigation_graph_model_get_relation_count(
            observation.graph_model
        ) == 3
    );

    relation_ab =
        investigation_graph_model_find_relation(
            observation.graph_model,
            TEST_RELATION_AB_IDENTIFIER
        );

    assert(relation_ab != NULL);

    assert(
        strcmp(
            relation_record_get_source_entity_identifier(
                relation_ab
            ),
            TEST_ENTITY_A_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_target_entity_identifier(
                relation_ab
            ),
            TEST_ENTITY_B_IDENTIFIER
        ) == 0
    );

    outgoing_relations =
        investigation_graph_model_list_outgoing_relations(
            observation.graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(outgoing_relations != NULL);
    assert(error == NULL);
    assert(outgoing_relations->len == 2);

    incoming_relations =
        investigation_graph_model_list_incoming_relations(
            observation.graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(incoming_relations != NULL);
    assert(error == NULL);
    assert(incoming_relations->len == 1);

    g_ptr_array_unref(
        incoming_relations
    );

    g_ptr_array_unref(
        outgoing_relations
    );

    investigation_graph_load_task_free(
        load_task
    );

    test_graph_load_task_observation_clear(
        &observation
    );

    test_graph_load_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'annulation avant l'ouverture SQLite.
 */
static void test_graph_load_task_cancel_before_open(void)
{
    TestGraphLoadTaskFixture fixture =
        test_graph_load_task_fixture_create();

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation observation =
        test_graph_load_task_observation_create();

    GError *error =
        NULL;

    test_graph_load_task_fixture_close_database(
        &fixture
    );

    investigation_graph_load_task_test_reset();
    investigation_graph_load_task_test_pause_before_open();

    load_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &observation
    );

    investigation_graph_load_task_test_wait_until_before_open();

    investigation_graph_load_task_cancel(
        load_task
    );

    investigation_graph_load_task_cancel(
        load_task
    );

    investigation_graph_load_task_test_release_before_open();

    test_graph_load_task_wait_for_callback(
        &observation
    );

    assert(observation.graph_model == NULL);

    test_graph_load_task_assert_error(
        observation.error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_CANCELLED
    );

    investigation_graph_load_task_free(
        load_task
    );

    test_graph_load_task_observation_clear(
        &observation
    );

    test_graph_load_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'annulation après l'ouverture de la connexion du worker.
 */
static void test_graph_load_task_cancel_after_open(void)
{
    TestGraphLoadTaskFixture fixture =
        test_graph_load_task_fixture_create();

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation observation =
        test_graph_load_task_observation_create();

    GError *error =
        NULL;

    test_graph_load_task_fixture_close_database(
        &fixture
    );

    investigation_graph_load_task_test_reset();
    investigation_graph_load_task_test_pause_after_open();

    load_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &observation
    );

    investigation_graph_load_task_test_wait_until_after_open();

    investigation_graph_load_task_cancel(
        load_task
    );

    investigation_graph_load_task_test_release_after_open();

    test_graph_load_task_wait_for_callback(
        &observation
    );

    assert(observation.graph_model == NULL);

    test_graph_load_task_assert_error(
        observation.error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_CANCELLED
    );

    investigation_graph_load_task_free(
        load_task
    );

    test_graph_load_task_observation_clear(
        &observation
    );

    test_graph_load_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'une destruction active supprime le callback utilisateur.
 */
static void test_graph_load_task_free_while_running(void)
{
    TestGraphLoadTaskFixture fixture =
        test_graph_load_task_fixture_create();

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation observation =
        test_graph_load_task_observation_create();

    GError *error =
        NULL;

    test_graph_load_task_fixture_close_database(
        &fixture
    );

    investigation_graph_load_task_test_reset();
    investigation_graph_load_task_test_pause_before_open();

    load_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &observation
    );

    investigation_graph_load_task_test_wait_until_before_open();

    investigation_graph_load_task_free(
        load_task
    );

    load_task =
        NULL;

    assert(observation.callback_count == 0);
    assert(observation.destroy_count == 1);

    investigation_graph_load_task_test_release_before_open();

    test_graph_load_task_wait_for_internal_completion();

    assert(observation.callback_count == 0);
    assert(observation.destroy_count == 1);
    assert(observation.graph_model == NULL);
    assert(observation.error == NULL);

    test_graph_load_task_observation_clear(
        &observation
    );

    test_graph_load_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la propagation d'un échec d'ouverture SQLite.
 */
static void test_graph_load_task_database_open_failure(void)
{
    char *temporary_directory =
        NULL;

    char *missing_directory =
        NULL;

    char *database_path =
        NULL;

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation observation =
        test_graph_load_task_observation_create();

    GError *error =
        NULL;

    temporary_directory =
        g_dir_make_tmp(
            "labfy-graph-open-failure-test-XXXXXX",
            &error
        );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    missing_directory =
        g_build_filename(
            temporary_directory,
            "repertoire-absent",
            NULL
        );

    database_path =
        g_build_filename(
            missing_directory,
            "Enquete.sqlite",
            NULL
        );

    investigation_graph_load_task_test_reset();

    load_task =
        investigation_graph_load_task_new(
            database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &observation
    );

    test_graph_load_task_wait_for_callback(
        &observation
    );

    assert(observation.graph_model == NULL);

    test_graph_load_task_assert_error(
        observation.error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_DATABASE_OPEN
    );

    assert(
        strstr(
            observation.error->message,
            database_path
        ) != NULL
    );

    investigation_graph_load_task_free(
        load_task
    );

    test_graph_load_task_observation_clear(
        &observation
    );

    assert(
        g_rmdir(
            temporary_directory
        ) == 0
    );

    g_free(
        database_path
    );

    g_free(
        missing_directory
    );

    g_free(
        temporary_directory
    );
}

/**
 * @brief Vérifie la propagation d'un échec provenant du chargeur synchrone.
 */
static void test_graph_load_task_load_failure(void)
{
    char *temporary_directory =
        NULL;

    char *database_path =
        NULL;

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation observation =
        test_graph_load_task_observation_create();

    GError *error =
        NULL;

    temporary_directory =
        g_dir_make_tmp(
            "labfy-graph-load-failure-test-XXXXXX",
            &error
        );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    database_path =
        g_build_filename(
            temporary_directory,
            "SansSchema.sqlite",
            NULL
        );

    assert(
        g_file_set_contents(
            database_path,
            "",
            0,
            &error
        )
    );

    assert(error == NULL);

    investigation_graph_load_task_test_reset();

    load_task =
        investigation_graph_load_task_new(
            database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &observation
    );

    test_graph_load_task_wait_for_callback(
        &observation
    );

    assert(observation.graph_model == NULL);

    test_graph_load_task_assert_error(
        observation.error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_LOAD
    );

    assert(
        strstr(
            observation.error->message,
            "graphe"
        ) != NULL
    );

    investigation_graph_load_task_free(
        load_task
    );

    test_graph_load_task_observation_clear(
        &observation
    );

    test_graph_load_task_remove_optional_file(
        database_path
    );

    assert(
        g_rmdir(
            temporary_directory
        ) == 0
    );

    g_free(
        database_path
    );

    g_free(
        temporary_directory
    );
}

/**
 * @brief Vérifie la réutilisation après une première finalisation.
 */
static void test_graph_load_task_reuse(void)
{
    TestGraphLoadTaskFixture fixture =
        test_graph_load_task_fixture_create();

    InvestigationGraphLoadTask *load_task =
        NULL;

    TestGraphLoadTaskObservation first_observation =
        test_graph_load_task_observation_create();

    TestGraphLoadTaskObservation second_observation =
        test_graph_load_task_observation_create();

    GError *error =
        NULL;

    test_graph_load_task_insert_entity(
        &fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_graph_load_task_fixture_close_database(
        &fixture
    );

    investigation_graph_load_task_test_reset();

    load_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(load_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        load_task,
        &first_observation
    );

    test_graph_load_task_wait_for_callback(
        &first_observation
    );

    assert(first_observation.graph_model != NULL);
    assert(first_observation.error == NULL);

    assert(
        !investigation_graph_load_task_is_running(
            load_task
        )
    );

    test_graph_load_task_start_or_abort(
        load_task,
        &second_observation
    );

    test_graph_load_task_wait_for_callback(
        &second_observation
    );

    assert(second_observation.graph_model != NULL);
    assert(second_observation.error == NULL);

    assert(
        first_observation.graph_model !=
        second_observation.graph_model
    );

    assert(
        investigation_graph_model_find_entity(
            first_observation.graph_model,
            TEST_ENTITY_A_IDENTIFIER
        ) !=
        investigation_graph_model_find_entity(
            second_observation.graph_model,
            TEST_ENTITY_A_IDENTIFIER
        )
    );

    investigation_graph_load_task_free(
        load_task
    );

    test_graph_load_task_observation_clear(
        &second_observation
    );

    test_graph_load_task_observation_clear(
        &first_observation
    );

    test_graph_load_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie deux tâches simultanées et deux connexions indépendantes.
 */
static void test_graph_load_task_two_simultaneous_tasks(void)
{
    TestGraphLoadTaskFixture fixture =
        test_graph_load_task_fixture_create();

    InvestigationGraphLoadTask *first_task =
        NULL;

    InvestigationGraphLoadTask *second_task =
        NULL;

    TestGraphLoadTaskObservation first_observation =
        {0};

    TestGraphLoadTaskObservation second_observation =
        {0};

    GMainLoop *shared_main_loop =
        NULL;

    guint timeout_source =
        0;

    GError *error =
        NULL;

    test_graph_load_task_insert_standard_graph(
        &fixture
    );

    test_graph_load_task_fixture_close_database(
        &fixture
    );

    shared_main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    assert(shared_main_loop != NULL);

    first_observation.main_loop =
        shared_main_loop;

    first_observation.main_thread =
        g_thread_self();

    second_observation.main_loop =
        shared_main_loop;

    second_observation.main_thread =
        g_thread_self();

    investigation_graph_load_task_test_reset();

    first_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(first_task != NULL);
    assert(error == NULL);

    second_task =
        investigation_graph_load_task_new(
            fixture.database_path,
            &error
        );

    assert(second_task != NULL);
    assert(error == NULL);

    test_graph_load_task_start_or_abort(
        first_task,
        &first_observation
    );

    test_graph_load_task_start_or_abort(
        second_task,
        &second_observation
    );

    timeout_source =
        g_timeout_add_seconds(
            5,
            test_graph_load_task_timeout,
            &first_observation
        );

    assert(timeout_source != 0);

    while (first_observation.callback_count == 0 ||
           second_observation.callback_count == 0)
    {
        g_main_loop_run(
            shared_main_loop
        );

        assert(!first_observation.timed_out);
    }

    assert(
        g_source_remove(
            timeout_source
        )
    );

    assert(first_observation.destroy_count == 1);
    assert(second_observation.destroy_count == 1);

    assert(first_observation.error == NULL);
    assert(second_observation.error == NULL);

    assert(first_observation.graph_model != NULL);
    assert(second_observation.graph_model != NULL);

    assert(
        first_observation.graph_model !=
        second_observation.graph_model
    );

    investigation_graph_load_task_free(
        second_task
    );

    investigation_graph_load_task_free(
        first_task
    );

    investigation_graph_model_free(
        second_observation.graph_model
    );

    investigation_graph_model_free(
        first_observation.graph_model
    );

    g_clear_error(
        &second_observation.error
    );

    g_clear_error(
        &first_observation.error
    );

    g_main_loop_unref(
        shared_main_loop
    );

    test_graph_load_task_fixture_clear(
        &fixture
    );
}

int main(
    int argc,
    char **argv
)
{
    g_test_init(
        &argc,
        &argv,
        NULL
    );

    /*
     * Certains scénarios provoquent volontairement des avertissements de la
     * couche SQLite. Ils ne doivent pas masquer les erreurs structurées
     * vérifiées par les tests.
     */
    g_log_set_always_fatal(
        G_LOG_FATAL_MASK
    );

    test_graph_load_task_lifecycle();
    test_graph_load_task_empty_database();
    test_graph_load_task_complete_graph();
    test_graph_load_task_cancel_before_open();
    test_graph_load_task_cancel_after_open();
    test_graph_load_task_free_while_running();
    test_graph_load_task_database_open_failure();
    test_graph_load_task_load_failure();
    test_graph_load_task_reuse();
    test_graph_load_task_two_simultaneous_tasks();

    printf(
        "InvestigationGraphLoadTask : tous les tests sont valides.\n"
    );

    return 0;
}
