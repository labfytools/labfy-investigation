/******************************************************************************
 * @file local_capability_registry.c
 * @brief Détection bornée et lookup sans effet métier des capacités J4.
 ******************************************************************************/
#include "core/local_capability_registry.h"

#include <glib/gstdio.h>
#include <string.h>

struct LocalCapabilityRegistry { GPtrArray *statuses; };

static const LocalCapabilityDescriptor descriptors[] = {
    {
        LOCAL_CAPABILITY_EML_HEADERS, "1", "labfy.adapter.eml.native", "1",
        "labfy.eml_analyzer", "Analyser les en-têtes EML", "message/",
        ".eml", "labfy.eml_analysis.derivative.v1",
        "labfy.local_tool_result.v1", LOCAL_ADAPTER_NATIVE, "PASSIVE", "NONE",
        {30000U, 250U, 30U, 256U * 1024U * 1024U, 4U * 1024U * 1024U,
            64U, 1024U * 1024U, 256U * 1024U}
    },
    {
        LOCAL_CAPABILITY_EXIF_METADATA, "1", "labfy.adapter.exiftool", "1",
        "exiftool", "Examiner les métadonnées", "image/", NULL,
        "labfy.exiftool.metadata.derivative.v1",
        "labfy.local_tool_result.v1", LOCAL_ADAPTER_SUBPROCESS, "PASSIVE", "NONE",
        {30000U, 250U, 30U, 256U * 1024U * 1024U, 4U * 1024U * 1024U,
            64U, 1024U * 1024U, 256U * 1024U}
    }
};

static void local_capability_status_free(gpointer data)
{
    LocalCapabilityStatus *status = data;
    if (status == NULL) return;
    g_free(status->reason); g_free(status->executable);
    g_free(status->tool_version); g_free(status);
}

static char *local_capability_bytes_text(GBytes *bytes)
{
    gsize size = 0U;
    const char *data = g_bytes_get_data(bytes, &size);
    char *text = g_utf8_make_valid(data, (gssize) size);
    g_strstrip(text);
    return text;
}

static void local_capability_probe_exiftool(LocalCapabilityStatus *status)
{
    char *found = g_find_program_in_path("exiftool");
    char *resolved = found != NULL ? g_canonicalize_filename(found, NULL) : NULL;
    g_free(found);
    if (resolved == NULL) {
        status->availability = LOCAL_CAPABILITY_MISSING;
        status->reason = g_strdup("ExifTool est absent du PATH contrôlé.");
        return;
    }
    char *temporary = g_dir_make_tmp("labfy-exif-probe-XXXXXX", NULL);
    const char *argv[] = { resolved, "-config", "", "-ver", NULL };
    LocalToolRunnerProfile probe = status->descriptor->limits;
    probe.wall_timeout_ms = 3000U;
    probe.cpu_seconds = 3U;
    probe.stdout_bytes = probe.stderr_bytes = 16U * 1024U;
    LocalToolRunnerResult *result = NULL;
    GError *error = NULL;
    if (temporary == NULL || !local_tool_runner_run(resolved, argv, temporary,
            &probe, NULL, &result, &error)) {
        status->availability = LOCAL_CAPABILITY_DETECTION_ERROR;
        status->reason = g_strdup(error != NULL ? error->message
            : "La sonde ExifTool n'a pas pu démarrer.");
    } else if (result->state != LOCAL_TOOL_RUNNER_EXITED ||
        !result->stdout_complete) {
        status->availability = result->state == LOCAL_TOOL_RUNNER_TIMEOUT
            ? LOCAL_CAPABILITY_UNVERIFIED : LOCAL_CAPABILITY_INCOMPATIBLE;
        status->reason = g_strdup("La sonde ExifTool bornée n'a pas produit une version compatible.");
    } else {
        status->tool_version = local_capability_bytes_text(result->stdout_bytes);
        if (status->tool_version[0] == '\0') {
            status->availability = LOCAL_CAPABILITY_INCOMPATIBLE;
            status->reason = g_strdup("ExifTool ne fournit pas de version exploitable.");
        } else {
            status->availability = LOCAL_CAPABILITY_READY;
            status->reason = g_strdup("ExifTool qualifié par une sonde bornée.");
            status->executable = g_strdup(resolved);
        }
    }
    local_tool_runner_result_free(result);
    g_clear_error(&error);
    if (temporary != NULL) { (void) g_rmdir(temporary); g_free(temporary); }
    g_free(resolved);
}

LocalCapabilityRegistry *local_capability_registry_new(GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    LocalCapabilityRegistry *registry = g_try_new0(LocalCapabilityRegistry, 1);
    if (registry == NULL) return NULL;
    registry->statuses = g_ptr_array_new_with_free_func(
        local_capability_status_free);
    for (guint index = 0U; index < G_N_ELEMENTS(descriptors); index++) {
        LocalCapabilityStatus *status = g_new0(LocalCapabilityStatus, 1);
        status->descriptor = &descriptors[index];
        if (status->descriptor->adapter_kind == LOCAL_ADAPTER_NATIVE) {
            status->availability = LOCAL_CAPABILITY_READY;
            status->tool_version = g_strdup("1");
            status->reason = g_strdup("Analyseur natif intégré au cœur C.");
        } else local_capability_probe_exiftool(status);
        g_ptr_array_add(registry->statuses, status);
    }
    return registry;
}

void local_capability_registry_free(LocalCapabilityRegistry *registry)
{
    if (registry == NULL) return;
    g_ptr_array_unref(registry->statuses); g_free(registry);
}

const LocalCapabilityStatus *local_capability_registry_lookup(
    const LocalCapabilityRegistry *registry, const char *capability_id)
{
    if (registry == NULL || capability_id == NULL) return NULL;
    for (guint index = 0U; index < registry->statuses->len; index++) {
        const LocalCapabilityStatus *status =
            g_ptr_array_index(registry->statuses, index);
        if (g_strcmp0(status->descriptor->capability_id, capability_id) == 0)
            return status;
    }
    return NULL;
}

const GPtrArray *local_capability_registry_get_statuses(
    const LocalCapabilityRegistry *registry)
{ return registry != NULL ? registry->statuses : NULL; }

gboolean local_capability_status_applies(
    const LocalCapabilityStatus *status, const char *mime_type,
    const char *relative_path, char **out_reason)
{
    if (out_reason != NULL) *out_reason = NULL;
    if (status == NULL || mime_type == NULL || relative_path == NULL) return FALSE;
    gboolean mime = status->descriptor->accepted_mime_prefix == NULL ||
        g_str_has_prefix(mime_type, status->descriptor->accepted_mime_prefix);
    gboolean extension = status->descriptor->accepted_extension == NULL ||
        g_str_has_suffix(relative_path, status->descriptor->accepted_extension);
    gboolean applies = mime && extension;
    if (out_reason != NULL) *out_reason = g_strdup(applies
        ? "La preuve contrôlée correspond au type accepté."
        : "Le type contrôlé de la preuve n'est pas accepté.");
    return applies;
}

const char *local_capability_availability_code(
    LocalCapabilityAvailability availability)
{
    static const char *const values[] = { "available", "missing",
        "incompatible", "unverified", "detection_error" };
    return availability <= LOCAL_CAPABILITY_DETECTION_ERROR
        ? values[availability] : "detection_error";
}
