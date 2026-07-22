/******************************************************************************
 * @file osint_execution_record.c
 * @brief Modèle immuable d'une exécution OSINT persistée.
 ******************************************************************************/

#include "models/osint_execution_record.h"

#include <string.h>

struct OsintExecutionRecord
{
    char *identifier;
    char *tool_identifier;
    char *tool_version;
    char *action_identifier;
    char *selection_identifier;
    char *selection_kind;
    char *target_value;
    char *arguments;
    char *started_at;
    char *finished_at;
    gboolean has_exit_code;
    gint exit_code;
    char *final_state;
    GBytes *stdout_raw;
    GBytes *stderr_raw;
    char *output_sha256;
};

static gboolean osint_execution_record_text_valid(const char *text)
{ return text != NULL && text[0] != '\0'; }

OsintExecutionRecord *osint_execution_record_new(
    const char *identifier, const char *tool_identifier,
    const char *tool_version, const char *action_identifier,
    const char *selection_identifier, const char *selection_kind,
    const char *target_value, const char *arguments,
    const char *started_at, const char *finished_at,
    gboolean has_exit_code, gint exit_code, const char *final_state,
    GBytes *stdout_raw, GBytes *stderr_raw, const char *output_sha256,
    GError **error
)
{
    OsintExecutionRecord *record = NULL;
    const gboolean selection_valid = g_strcmp0(selection_kind, "entity") == 0 ||
        g_strcmp0(selection_kind, "relation") == 0;
    const gboolean state_valid = g_strcmp0(final_state, "completed") == 0 ||
        g_strcmp0(final_state, "failed") == 0 ||
        g_strcmp0(final_state, "cancelled") == 0;
    if ((error != NULL && *error != NULL) || identifier == NULL ||
        !g_uuid_string_is_valid(identifier) || selection_identifier == NULL ||
        !g_uuid_string_is_valid(selection_identifier) || !selection_valid ||
        !state_valid || !osint_execution_record_text_valid(tool_identifier) ||
        !osint_execution_record_text_valid(action_identifier) ||
        !osint_execution_record_text_valid(target_value) || arguments == NULL ||
        started_at == NULL || strlen(started_at) != 20U || finished_at == NULL ||
        strlen(finished_at) != 20U || stdout_raw == NULL || stderr_raw == NULL ||
        output_sha256 == NULL || strlen(output_sha256) != 64U)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "L'exécution OSINT est invalide.");
        return NULL;
    }
    record = g_try_new0(OsintExecutionRecord, 1);
    if (record == NULL) return NULL;
    record->identifier = g_strdup(identifier);
    record->tool_identifier = g_strdup(tool_identifier);
    record->tool_version = g_strdup(tool_version);
    record->action_identifier = g_strdup(action_identifier);
    record->selection_identifier = g_strdup(selection_identifier);
    record->selection_kind = g_strdup(selection_kind);
    record->target_value = g_strdup(target_value);
    record->arguments = g_strdup(arguments);
    record->started_at = g_strdup(started_at);
    record->finished_at = g_strdup(finished_at);
    record->has_exit_code = has_exit_code;
    record->exit_code = exit_code;
    record->final_state = g_strdup(final_state);
    record->stdout_raw = g_bytes_ref(stdout_raw);
    record->stderr_raw = g_bytes_ref(stderr_raw);
    record->output_sha256 = g_strdup(output_sha256);
    if (record->identifier == NULL || record->tool_identifier == NULL ||
        record->action_identifier == NULL || record->selection_identifier == NULL ||
        record->selection_kind == NULL || record->target_value == NULL ||
        record->arguments == NULL || record->started_at == NULL ||
        record->finished_at == NULL || record->final_state == NULL ||
        record->output_sha256 == NULL)
    {
        osint_execution_record_free(record);
        return NULL;
    }
    return record;
}

void osint_execution_record_free(OsintExecutionRecord *record)
{
    if (record == NULL) return;
    g_free(record->output_sha256); g_clear_pointer(&record->stderr_raw, g_bytes_unref);
    g_clear_pointer(&record->stdout_raw, g_bytes_unref); g_free(record->final_state);
    g_free(record->finished_at); g_free(record->started_at); g_free(record->arguments);
    g_free(record->target_value); g_free(record->selection_kind);
    g_free(record->selection_identifier); g_free(record->action_identifier);
    g_free(record->tool_version); g_free(record->tool_identifier);
    g_free(record->identifier); g_free(record);
}

#define OSINT_GETTER(name, field) \
const char *name(const OsintExecutionRecord *record) \
{ return record != NULL ? record->field : NULL; }
OSINT_GETTER(osint_execution_record_get_identifier, identifier)
OSINT_GETTER(osint_execution_record_get_tool_identifier, tool_identifier)
OSINT_GETTER(osint_execution_record_get_tool_version, tool_version)
OSINT_GETTER(osint_execution_record_get_action_identifier, action_identifier)
OSINT_GETTER(osint_execution_record_get_selection_identifier, selection_identifier)
OSINT_GETTER(osint_execution_record_get_selection_kind, selection_kind)
OSINT_GETTER(osint_execution_record_get_target_value, target_value)
OSINT_GETTER(osint_execution_record_get_arguments, arguments)
OSINT_GETTER(osint_execution_record_get_started_at, started_at)
OSINT_GETTER(osint_execution_record_get_finished_at, finished_at)
OSINT_GETTER(osint_execution_record_get_final_state, final_state)
OSINT_GETTER(osint_execution_record_get_output_sha256, output_sha256)
#undef OSINT_GETTER
gboolean osint_execution_record_has_exit_code(const OsintExecutionRecord *record)
{ return record != NULL && record->has_exit_code; }
gint osint_execution_record_get_exit_code(const OsintExecutionRecord *record)
{ return record != NULL && record->has_exit_code ? record->exit_code : -1; }
GBytes *osint_execution_record_ref_stdout(const OsintExecutionRecord *record)
{ return record != NULL && record->stdout_raw != NULL ? g_bytes_ref(record->stdout_raw) : NULL; }
GBytes *osint_execution_record_ref_stderr(const OsintExecutionRecord *record)
{ return record != NULL && record->stderr_raw != NULL ? g_bytes_ref(record->stderr_raw) : NULL; }
