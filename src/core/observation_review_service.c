#include "core/observation_review_service.h"

#include "database/error.h"
#include "database/statement.h"
#include "database/transaction.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <string.h>

struct ObservationReviewService { Database *database; };

typedef struct {
    char *evidence_id;
    char *entity_id;
    char *entity_type;
    char *value_raw;
    char *value_normalized;
    char *value_corrected;
    char *verification_status;
    char *extraction_id;
} ObservationSnapshot;

GQuark observation_review_error_quark(void)
{
    return g_quark_from_static_string("observation-review-error-quark");
}

static void set_error(GError **error, ObservationReviewError code,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, OBSERVATION_REVIEW_ERROR, code, message);
}

static void set_database_error(ObservationReviewService *service,
    GError **error, const char *context)
{
    const char *detail = service != NULL
        ? database_error_get_message(service->database) : NULL;
    if (error != NULL && *error == NULL)
        g_set_error(error, OBSERVATION_REVIEW_ERROR,
            OBSERVATION_REVIEW_ERROR_DATABASE, "%s%s%s", context,
            detail != NULL ? " : " : "", detail != NULL ? detail : "");
}

static void snapshot_clear(ObservationSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    g_free(snapshot->evidence_id); g_free(snapshot->entity_id);
    g_free(snapshot->entity_type); g_free(snapshot->value_raw);
    g_free(snapshot->value_normalized); g_free(snapshot->value_corrected);
    g_free(snapshot->verification_status); g_free(snapshot->extraction_id);
    memset(snapshot, 0, sizeof(*snapshot));
}

static const char *action_name(ObservationReviewAction action)
{
    switch (action) {
    case OBSERVATION_REVIEW_ACTION_DECIDE: return "decision";
    case OBSERVATION_REVIEW_ACTION_CORRECT: return "correction";
    case OBSERVATION_REVIEW_ACTION_PROMOTE_CREATE: return "promotion_create";
    case OBSERVATION_REVIEW_ACTION_PROMOTE_ATTACH: return "promotion_attach";
    case OBSERVATION_REVIEW_ACTION_WITHDRAW_PROMOTION: return "promotion_withdraw";
    default: return NULL;
    }
}

static gboolean validate_request(ObservationReviewService *service,
    const ObservationReviewRequest *request, ObservationReviewResult *result,
    GError **error)
{
    const char *name = request != NULL ? action_name(request->action) : NULL;
    if (result != NULL) memset(result, 0, sizeof(*result));
    if (service == NULL || service->database == NULL || request == NULL ||
        result == NULL || !g_uuid_string_is_valid(request->operation_identifier) ||
        !g_uuid_string_is_valid(request->evidence_identifier) ||
        !g_uuid_string_is_valid(request->observation_identifier) || name == NULL ||
        request->author == NULL || request->author[0] == '\0' ||
        request->reason == NULL || request->reason[0] == '\0' ||
        request->occurred_at == NULL || strlen(request->occurred_at) != 20U)
    {
        set_error(error, OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
            "La demande de revue humaine est invalide.");
        return FALSE;
    }
    if (request->action == OBSERVATION_REVIEW_ACTION_DECIDE &&
        g_strcmp0(request->verification_status, "proposed") != 0 &&
        g_strcmp0(request->verification_status, "confirmed") != 0 &&
        g_strcmp0(request->verification_status, "rejected") != 0 &&
        g_strcmp0(request->verification_status, "conflicted") != 0)
    {
        set_error(error, OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
            "L'état de revue demandé est invalide.");
        return FALSE;
    }
    if (request->action == OBSERVATION_REVIEW_ACTION_CORRECT &&
        (request->corrected_value == NULL || request->corrected_value[0] == '\0'))
    {
        set_error(error, OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
            "La correction humaine est absente.");
        return FALSE;
    }
    if (request->action == OBSERVATION_REVIEW_ACTION_PROMOTE_ATTACH &&
        !g_uuid_string_is_valid(request->entity_identifier))
    {
        set_error(error, OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
            "L'entité à rattacher est invalide.");
        return FALSE;
    }
    return TRUE;
}

static char *build_intent(const ObservationReviewRequest *request)
{
    JsonBuilder *builder = json_builder_new();
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = NULL; char *json = NULL;
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "version");
    json_builder_add_int_value(builder, 1);
    json_builder_set_member_name(builder, "operation_id");
    json_builder_add_string_value(builder, request->operation_identifier);
    json_builder_set_member_name(builder, "evidence_id");
    json_builder_add_string_value(builder, request->evidence_identifier);
    json_builder_set_member_name(builder, "observation_id");
    json_builder_add_string_value(builder, request->observation_identifier);
    json_builder_set_member_name(builder, "action");
    json_builder_add_string_value(builder, action_name(request->action));
    json_builder_set_member_name(builder, "expected_revision");
    json_builder_add_int_value(builder, (gint64) request->expected_revision);
    json_builder_set_member_name(builder, "new_state");
    if (request->verification_status != NULL)
        json_builder_add_string_value(builder, request->verification_status);
    else json_builder_add_null_value(builder);
    json_builder_set_member_name(builder, "corrected_value");
    if (request->corrected_value != NULL)
        json_builder_add_string_value(builder, request->corrected_value);
    else json_builder_add_null_value(builder);
    json_builder_set_member_name(builder, "requested_entity_id");
    if (request->entity_identifier != NULL)
        json_builder_add_string_value(builder, request->entity_identifier);
    else json_builder_add_null_value(builder);
    json_builder_set_member_name(builder, "author");
    json_builder_add_string_value(builder, request->author);
    json_builder_set_member_name(builder, "reason");
    json_builder_add_string_value(builder, request->reason);
    json_builder_end_object(builder);
    root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    json = json_generator_to_data(generator, NULL);
    json_node_free(root); g_object_unref(generator); g_object_unref(builder);
    return json;
}

static gboolean read_existing_operation(ObservationReviewService *service,
    const ObservationReviewRequest *request, const char *intent,
    ObservationReviewResult *result, gboolean *out_found, GError **error)
{
    DatabaseStatement *statement = database_statement_prepare(service->database,
        "SELECT details FROM journal WHERE id=?;");
    DatabaseStatementStepResult step; char *details = NULL;
    JsonParser *parser = NULL; JsonObject *object = NULL;
    const char *stored_intent = NULL, *entity_id = NULL;
    gint64 revision = 0; gboolean created = FALSE;
    *out_found = FALSE;
    if (statement == NULL || !database_statement_bind_text(statement, 1,
            request->operation_identifier)) goto database_failure;
    step = database_statement_step(statement);
    if (step == DATABASE_STATEMENT_STEP_DONE) {
        database_statement_finalize(statement); return TRUE;
    }
    if (step != DATABASE_STATEMENT_STEP_ROW ||
        !database_statement_column_text(statement, 0, &details))
        goto database_failure;
    database_statement_finalize(statement); statement = NULL;
    parser = json_parser_new();
    if (details == NULL || !json_parser_load_from_data(parser, details, -1, NULL) ||
        !JSON_NODE_HOLDS_OBJECT(json_parser_get_root(parser))) goto conflict;
    object = json_node_get_object(json_parser_get_root(parser));
    stored_intent = json_object_get_string_member_with_default(object,
        "intent", NULL);
    if (g_strcmp0(stored_intent, intent) != 0) goto conflict;
    revision = json_object_get_int_member_with_default(object, "revision", -1);
    entity_id = json_object_get_string_member_with_default(object,
        "entity_id", NULL);
    created = json_object_get_boolean_member_with_default(object,
        "entity_created", FALSE);
    if (revision < 0) goto conflict;
    result->replayed = TRUE; result->revision = (guint64) revision;
    result->entity_identifier = g_strdup(entity_id);
    result->entity_created = created;
    *out_found = TRUE;
    g_object_unref(parser); g_free(details);
    return TRUE;
conflict:
    g_clear_object(&parser); g_free(details);
    set_error(error, OBSERVATION_REVIEW_ERROR_CONFLICT,
        "L'UUID d'opération existe avec une intention différente.");
    return FALSE;
database_failure:
    database_statement_finalize(statement); g_free(details);
    set_database_error(service, error, "Impossible de relire l'opération");
    return FALSE;
}

static gboolean load_snapshot(ObservationReviewService *service,
    const ObservationReviewRequest *request, ObservationSnapshot *snapshot,
    GError **error)
{
    DatabaseStatement *statement = database_statement_prepare(service->database,
        "SELECT evidence_id,entity_id,entity_type,value_raw,value_normalized,"
        "value_corrected,verification_status,extraction_id "
        "FROM evidence_entity_observations WHERE id=? AND evidence_id=?;");
    DatabaseStatementStepResult step;
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, request->observation_identifier) ||
        !database_statement_bind_text(statement, 2, request->evidence_identifier))
        goto database_failure;
    step = database_statement_step(statement);
    if (step == DATABASE_STATEMENT_STEP_DONE) {
        database_statement_finalize(statement);
        set_error(error, OBSERVATION_REVIEW_ERROR_NOT_FOUND,
            "L'observation n'appartient pas à la preuve demandée.");
        return FALSE;
    }
    if (step != DATABASE_STATEMENT_STEP_ROW ||
        !database_statement_column_text(statement, 0, &snapshot->evidence_id) ||
        !database_statement_column_text(statement, 1, &snapshot->entity_id) ||
        !database_statement_column_text(statement, 2, &snapshot->entity_type) ||
        !database_statement_column_text(statement, 3, &snapshot->value_raw) ||
        !database_statement_column_text(statement, 4, &snapshot->value_normalized) ||
        !database_statement_column_text(statement, 5, &snapshot->value_corrected) ||
        !database_statement_column_text(statement, 6, &snapshot->verification_status) ||
        !database_statement_column_text(statement, 7, &snapshot->extraction_id))
        goto database_failure;
    database_statement_finalize(statement);
    return TRUE;
database_failure:
    database_statement_finalize(statement);
    set_database_error(service, error, "Impossible de charger l'observation");
    return FALSE;
}

static gboolean read_revision(ObservationReviewService *service,
    const char *observation_id, guint64 *revision, GError **error)
{
    /* WHY: V20 has no observation revision column and this tranche forbids DDL.
     * The append-only review stream is therefore the authoritative revision. */
    DatabaseStatement *statement = database_statement_prepare(service->database,
        "SELECT COUNT(*) FROM journal WHERE action='observation.review.v1' "
        "AND objet_type='evidence_observation' AND objet_id=?;");
    int64_t count = 0; gboolean success = statement != NULL &&
        database_statement_bind_text(statement, 1, observation_id) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW &&
        database_statement_column_int64(statement, 0, &count) && count >= 0;
    database_statement_finalize(statement);
    if (!success) set_database_error(service, error,
        "Impossible de lire la révision de l'observation");
    else *revision = (guint64) count;
    return success;
}

static char *normalize_promotable(const ObservationSnapshot *snapshot,
    GError **error)
{
    const char *source = snapshot->value_corrected != NULL
        ? snapshot->value_corrected : snapshot->value_normalized;
    char *value = NULL;
    if (g_strcmp0(snapshot->entity_type, "email_address") == 0 ||
        g_strcmp0(snapshot->entity_type, "domain_name") == 0) {
        value = source != NULL ? g_utf8_strdown(source, -1) : NULL;
        if (value != NULL) g_strstrip(value);
    } else if (g_strcmp0(snapshot->entity_type, "ip_address") == 0) {
        GInetAddress *address = source != NULL
            ? g_inet_address_new_from_string(source) : NULL;
        if (address != NULL) {
            value = g_inet_address_to_string(address); g_object_unref(address);
        }
    } else {
        set_error(error, OBSERVATION_REVIEW_ERROR_UNSUPPORTED_TYPE,
            "Seuls e-mail, domaine et IP peuvent être promus.");
        return NULL;
    }
    if (value == NULL || value[0] == '\0') {
        g_free(value);
        set_error(error, OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
            "La valeur normalisée à promouvoir est invalide.");
        return NULL;
    }
    return value;
}

static gboolean execute_bound(ObservationReviewService *service,
    const char *sql, const char *const *values, gsize count, GError **error)
{
    DatabaseStatement *statement = database_statement_prepare(service->database, sql);
    gboolean success = statement != NULL;
    for (gsize index = 0; success && index < count; index++)
        success = values[index] == NULL || database_statement_bind_text(
            statement, (int) index + 1, values[index]);
    success = success && database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE;
    database_statement_finalize(statement);
    if (!success) set_database_error(service, error,
        "Impossible d'appliquer la mutation de revue");
    return success;
}

static gboolean mutate_simple(ObservationReviewService *service,
    const ObservationReviewRequest *request, const ObservationSnapshot *snapshot,
    ObservationReviewResult *result, GError **error)
{
    const char *values[4];
    if (request->action == OBSERVATION_REVIEW_ACTION_DECIDE) {
        values[0] = request->verification_status;
        values[1] = request->observation_identifier;
        return execute_bound(service,
            "UPDATE evidence_entity_observations SET verification_status=? "
            "WHERE id=?;", values, 2, error);
    }
    if (request->action == OBSERVATION_REVIEW_ACTION_CORRECT) {
        values[0] = request->corrected_value;
        values[1] = request->observation_identifier;
        return execute_bound(service,
            "UPDATE evidence_entity_observations SET value_corrected=? WHERE id=?;",
            values, 2, error);
    }
    if (request->action == OBSERVATION_REVIEW_ACTION_WITHDRAW_PROMOTION) {
        if (snapshot->entity_id == NULL) {
            set_error(error, OBSERVATION_REVIEW_ERROR_CONFLICT,
                "L'observation n'est pas promue."); return FALSE;
        }
        values[0] = snapshot->evidence_id; values[1] = snapshot->entity_id;
        values[2] = request->observation_identifier;
        if (!execute_bound(service,
                "DELETE FROM preuve_entite_sources WHERE preuve_id=? AND entite_id=? "
                "AND source_kind='eml_observation' AND source_uuid=?;",
                values, 3, error)) return FALSE;
        if (!execute_bound(service,
                "DELETE FROM preuve_entites WHERE preuve_id=? AND entite_id=? "
                "AND NOT EXISTS(SELECT 1 FROM preuve_entite_sources s WHERE "
                "s.preuve_id=? AND s.entite_id=?);",
                (const char *const[]){snapshot->evidence_id, snapshot->entity_id,
                    snapshot->evidence_id, snapshot->entity_id}, 4, error)) return FALSE;
        values[0] = request->observation_identifier;
        result->entity_identifier = g_strdup(snapshot->entity_id);
        return execute_bound(service,
            "UPDATE evidence_entity_observations SET entity_id=NULL,promoted_at=NULL,"
            "promotion_kind=NULL WHERE id=?;", values, 1, error);
    }
    return TRUE;
}

static gboolean read_entity_match(ObservationReviewService *service,
    const char *entity_id, const char *type, const char *value,
    gboolean *found, GError **error)
{
    DatabaseStatement *statement = database_statement_prepare(service->database,
        "SELECT 1 FROM entites e JOIN types_entite t ON t.id=e.type_id "
        "WHERE e.id=? AND t.code=? AND e.valeur=? AND e.status<>'deleted';");
    DatabaseStatementStepResult step;
    *found = FALSE;
    if (statement == NULL || !database_statement_bind_text(statement, 1, entity_id) ||
        !database_statement_bind_text(statement, 2, type) ||
        !database_statement_bind_text(statement, 3, value)) goto failure;
    step = database_statement_step(statement);
    if (step == DATABASE_STATEMENT_STEP_ROW) *found = TRUE;
    else if (step != DATABASE_STATEMENT_STEP_DONE) goto failure;
    database_statement_finalize(statement); return TRUE;
failure:
    database_statement_finalize(statement);
    set_database_error(service, error, "Impossible de vérifier l'entité");
    return FALSE;
}

static gboolean promote(ObservationReviewService *service,
    const ObservationReviewRequest *request, const ObservationSnapshot *snapshot,
    ObservationReviewResult *result, GError **error)
{
    char *normalized = normalize_promotable(snapshot, error);
    char *entity_id = NULL, *source_id = NULL; gboolean found = FALSE;
    if (normalized == NULL) return FALSE;
    if (snapshot->entity_id != NULL) {
        set_error(error, OBSERVATION_REVIEW_ERROR_CONFLICT,
            "L'observation est déjà promue."); goto failure;
    }
    if (request->action == OBSERVATION_REVIEW_ACTION_PROMOTE_ATTACH) {
        entity_id = g_strdup(request->entity_identifier);
        if (!read_entity_match(service, entity_id, snapshot->entity_type,
                normalized, &found, error)) goto failure;
        if (!found) {
            set_error(error, OBSERVATION_REVIEW_ERROR_CONFLICT,
                "L'entité ne correspond pas à la valeur normalisée."); goto failure;
        }
    } else {
        DatabaseStatement *statement = database_statement_prepare(service->database,
            "SELECT e.id FROM entites e JOIN types_entite t ON t.id=e.type_id "
            "WHERE t.code=? AND e.valeur=? LIMIT 1;");
        DatabaseStatementStepResult step;
        if (statement == NULL ||
            !database_statement_bind_text(statement, 1, snapshot->entity_type) ||
            !database_statement_bind_text(statement, 2, normalized)) {
            database_statement_finalize(statement); set_database_error(service,
                error, "Impossible de vérifier l'unicité de l'entité"); goto failure;
        }
        step = database_statement_step(statement);
        if (step == DATABASE_STATEMENT_STEP_ROW) {
            database_statement_column_text(statement, 0, &entity_id);
            database_statement_finalize(statement);
            set_error(error, OBSERVATION_REVIEW_ERROR_CONFLICT,
                "Une entité normalisée existe déjà ; utilisez un rattachement explicite.");
            goto failure;
        }
        if (step != DATABASE_STATEMENT_STEP_DONE) {
            database_statement_finalize(statement);
            set_database_error(service, error,
                "Impossible de vérifier l'unicité de l'entité");
            goto failure;
        }
        database_statement_finalize(statement); entity_id = g_uuid_string_random();
        if (!execute_bound(service,
                "INSERT INTO entites(id,type_id,valeur,confiance,created_at,updated_at,status) "
                "SELECT ?,id,?,50,?,?,'active' FROM types_entite WHERE code=?;",
                (const char *const[]){entity_id, normalized, request->occurred_at,
                    request->occurred_at, snapshot->entity_type}, 5, error)) goto failure;
        result->entity_created = TRUE;
    }
    if (!execute_bound(service,
            "INSERT OR IGNORE INTO preuve_entites(preuve_id,entite_id) VALUES(?,?);",
            (const char *const[]){snapshot->evidence_id, entity_id}, 2, error))
        goto failure;
    source_id = g_uuid_string_random();
    if (!execute_bound(service,
            "INSERT INTO preuve_entite_sources(id,preuve_id,entite_id,source_kind,"
            "source_uuid,created_at) VALUES(?,?,?,'eml_observation',?,?);",
            (const char *const[]){source_id, snapshot->evidence_id, entity_id,
                request->observation_identifier, request->occurred_at}, 5, error) ||
        !execute_bound(service,
            "UPDATE evidence_entity_observations SET entity_id=?,promoted_at=?,"
            "promotion_kind=? WHERE id=?;",
            (const char *const[]){entity_id, request->occurred_at,
                result->entity_created ? "created" : "reused",
                request->observation_identifier}, 4, error)) goto failure;
    result->entity_identifier = g_strdup(entity_id);
    g_free(source_id); g_free(entity_id); g_free(normalized); return TRUE;
failure:
    g_free(source_id); g_free(entity_id); g_free(normalized); return FALSE;
}

static char *build_journal_details(const ObservationReviewRequest *request,
    const ObservationSnapshot *snapshot, const ObservationReviewResult *result,
    const char *intent)
{
    JsonBuilder *b = json_builder_new(); JsonGenerator *g = json_generator_new();
    JsonNode *root; char *data;
    json_builder_begin_object(b);
    json_builder_set_member_name(b, "schema");
    json_builder_add_string_value(b, "observation-review-v1");
    json_builder_set_member_name(b, "intent"); json_builder_add_string_value(b, intent);
    json_builder_set_member_name(b, "operation_id");
    json_builder_add_string_value(b, request->operation_identifier);
    json_builder_set_member_name(b, "action");
    json_builder_add_string_value(b, action_name(request->action));
    json_builder_set_member_name(b, "old_state");
    json_builder_add_string_value(b, snapshot->verification_status);
    json_builder_set_member_name(b, "new_state");
    json_builder_add_string_value(b, request->action == OBSERVATION_REVIEW_ACTION_DECIDE
        ? request->verification_status : snapshot->verification_status);
    json_builder_set_member_name(b, "author");
    json_builder_add_string_value(b, request->author);
    json_builder_set_member_name(b, "reason");
    json_builder_add_string_value(b, request->reason);
    json_builder_set_member_name(b, "revision");
    json_builder_add_int_value(b, (gint64) result->revision);
    json_builder_set_member_name(b, "entity_id");
    if (result->entity_identifier != NULL)
        json_builder_add_string_value(b, result->entity_identifier);
    else json_builder_add_null_value(b);
    json_builder_set_member_name(b, "entity_created");
    json_builder_add_boolean_value(b, result->entity_created);
    json_builder_set_member_name(b, "references"); json_builder_begin_object(b);
    json_builder_set_member_name(b, "evidence_id");
    json_builder_add_string_value(b, snapshot->evidence_id);
    json_builder_set_member_name(b, "observation_id");
    json_builder_add_string_value(b, request->observation_identifier);
    json_builder_set_member_name(b, "extraction_id");
    if (snapshot->extraction_id != NULL)
        json_builder_add_string_value(b, snapshot->extraction_id);
    else json_builder_add_null_value(b);
    json_builder_end_object(b); json_builder_end_object(b);
    root = json_builder_get_root(b); json_generator_set_root(g, root);
    data = json_generator_to_data(g, NULL);
    json_node_free(root); g_object_unref(g); g_object_unref(b); return data;
}

static gboolean append_journal(ObservationReviewService *service,
    const ObservationReviewRequest *request, const ObservationSnapshot *snapshot,
    const ObservationReviewResult *result, const char *intent, GError **error)
{
    char *details = build_journal_details(request, snapshot, result, intent);
    gboolean success = details != NULL && execute_bound(service,
        "INSERT INTO journal(id,event_time,action,objet_type,objet_id,resultat,"
        "details,acteur,created_at) VALUES(?,?,'observation.review.v1',"
        "'evidence_observation',?,'success',?,?,?);",
        (const char *const[]){request->operation_identifier, request->occurred_at,
            request->observation_identifier, details, request->author,
            request->occurred_at}, 6, error);
    g_free(details); return success;
}

ObservationReviewService *observation_review_service_new(
    Database *database, GError **error)
{
    ObservationReviewService *service;
    if (database == NULL) {
        set_error(error, OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
            "La connexion Database est absente."); return NULL;
    }
    service = g_try_new0(ObservationReviewService, 1);
    if (service == NULL) {
        set_error(error, OBSERVATION_REVIEW_ERROR_DATABASE,
            "Impossible d'allouer le service de revue."); return NULL;
    }
    service->database = database; return service;
}

void observation_review_service_free(ObservationReviewService *service)
{ g_free(service); }

void observation_review_result_clear(ObservationReviewResult *result)
{
    if (result == NULL) return;
    g_free(result->entity_identifier); memset(result, 0, sizeof(*result));
}

gboolean observation_review_service_apply(ObservationReviewService *service,
    const ObservationReviewRequest *request, ObservationReviewResult *result,
    GError **error)
{
    ObservationSnapshot snapshot = {0}; char *intent = NULL;
    gboolean found = FALSE, success = FALSE, transaction = FALSE;
    guint64 revision = 0;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (!validate_request(service, request, result, error)) return FALSE;
    intent = build_intent(request);
    if (intent == NULL) { set_error(error, OBSERVATION_REVIEW_ERROR_DATABASE,
        "Impossible de sérialiser l'intention de revue."); goto cleanup; }
    if (!database_transaction_begin(service->database)) {
        set_database_error(service, error, "Impossible de démarrer la transaction");
        goto cleanup;
    }
    transaction = TRUE;
    /* INVARIANT: idempotence is checked before optimistic revision staleness. */
    if (!read_existing_operation(service, request, intent, result, &found, error))
        goto cleanup;
    if (found) { success = database_transaction_commit(service->database);
        transaction = !success; if (!success) set_database_error(service, error,
            "Impossible de terminer le rejeu idempotent"); goto cleanup; }
    if (!load_snapshot(service, request, &snapshot, error) ||
        !read_revision(service, request->observation_identifier, &revision, error))
        goto cleanup;
    if (revision != request->expected_revision) {
        set_error(error, OBSERVATION_REVIEW_ERROR_CONFLICT,
            "La révision attendue est périmée."); goto cleanup;
    }
    result->revision = revision + 1U;
    /* CONTRACT: only review projection columns are mutable; raw, normalized,
     * observation UUID and extraction UUID never appear in an UPDATE SET. */
    if ((request->action == OBSERVATION_REVIEW_ACTION_PROMOTE_CREATE ||
         request->action == OBSERVATION_REVIEW_ACTION_PROMOTE_ATTACH)
            ? !promote(service, request, &snapshot, result, error)
            : !mutate_simple(service, request, &snapshot, result, error))
        goto cleanup;
    if (!append_journal(service, request, &snapshot, result, intent, error))
        goto cleanup;
    if (!database_transaction_commit(service->database)) {
        set_database_error(service, error, "Impossible de valider la revue");
        goto cleanup;
    }
    transaction = FALSE; success = TRUE;
cleanup:
    if (transaction) database_transaction_rollback(service->database);
    if (!success) observation_review_result_clear(result);
    snapshot_clear(&snapshot); g_free(intent); return success;
}

gboolean observation_review_service_validate_replay(
    ObservationReviewService *service, const char *evidence_identifier,
    const char *observation_identifier, const char *extraction_identifier,
    const char *entity_type, const char *value_raw, const char *value_normalized,
    const char *role, const char *provenance_kind, const char *source_header,
    guint occurrence, gboolean *out_accepted, GError **error)
{
    static const char *const sql =
        "SELECT CASE WHEN o.evidence_id=?2 AND o.extraction_id=?3 AND "
        "o.entity_type=?4 AND o.value_raw=?5 AND o.value_normalized=?6 AND "
        "o.role=?7 AND o.provenance_kind=?8 AND o.source_header=?9 AND "
        "o.occurrence=?10 AND ((o.verification_status='proposed' AND "
        "o.value_corrected IS NULL AND o.entity_id IS NULL) OR EXISTS(SELECT 1 "
        "FROM journal j WHERE j.action='observation.review.v1' AND "
        "j.objet_type='evidence_observation' AND j.objet_id=o.id)) "
        "THEN 1 ELSE 0 END FROM evidence_entity_observations o WHERE o.id=?1;";
    DatabaseStatement *statement = NULL; DatabaseStatementStepResult step;
    int64_t accepted = 0;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_accepted != NULL) *out_accepted = FALSE;
    if (service == NULL || service->database == NULL || out_accepted == NULL ||
        !g_uuid_string_is_valid(evidence_identifier) ||
        !g_uuid_string_is_valid(observation_identifier) ||
        !g_uuid_string_is_valid(extraction_identifier) || entity_type == NULL ||
        value_raw == NULL || value_normalized == NULL || role == NULL ||
        provenance_kind == NULL || source_header == NULL || occurrence == 0U)
    {
        set_error(error, OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
            "Les références du rejeu d'observation sont invalides.");
        return FALSE;
    }
    statement = database_statement_prepare(service->database, sql);
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, observation_identifier) ||
        !database_statement_bind_text(statement, 2, evidence_identifier) ||
        !database_statement_bind_text(statement, 3, extraction_identifier) ||
        !database_statement_bind_text(statement, 4, entity_type) ||
        !database_statement_bind_text(statement, 5, value_raw) ||
        !database_statement_bind_text(statement, 6, value_normalized) ||
        !database_statement_bind_text(statement, 7, role) ||
        !database_statement_bind_text(statement, 8, provenance_kind) ||
        !database_statement_bind_text(statement, 9, source_header) ||
        !database_statement_bind_int64(statement, 10, occurrence))
        goto failure;
    step = database_statement_step(statement);
    if (step == DATABASE_STATEMENT_STEP_DONE) {
        database_statement_finalize(statement);
        set_error(error, OBSERVATION_REVIEW_ERROR_NOT_FOUND,
            "L'observation rejouée est absente.");
        return FALSE;
    }
    if (step != DATABASE_STATEMENT_STEP_ROW ||
        !database_statement_column_int64(statement, 0, &accepted)) goto failure;
    database_statement_finalize(statement);
    /* INVARIANT: journaled mutable decisions never excuse an immutable mismatch. */
    *out_accepted = accepted == 1;
    return TRUE;
failure:
    database_statement_finalize(statement);
    set_database_error(service, error, "Impossible de valider le rejeu");
    return FALSE;
}
