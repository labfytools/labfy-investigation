/******************************************************************************
 * @file exiftool_analysis.c
 * @brief Analyse ExifTool structurée et traçable.
 ******************************************************************************/
#include "core/exiftool_analysis.h"
#include "core/document_tool_runner.h"

#include <json-glib/json-glib.h>
#include <string.h>

#define EXIFTOOL_MAX_TAGS 2048U
#define EXIFTOOL_MAX_DEPTH 16U

typedef struct
{
    const char *tag;
    const char *code;
    gboolean sensitive;
} ExiftoolMapping;

static const ExiftoolMapping exiftool_mappings[] = {
    { "File:MIMEType", "file.mime_type", FALSE },
    { "File:FileSize", "file.size_bytes", FALSE },
    { "File:FileTypeExtension", "file.detected_extension", FALSE },
    { "EXIF:ImageWidth", "image.width", FALSE },
    { "EXIF:ImageHeight", "image.height", FALSE },
    { "EXIF:Orientation", "image.orientation", FALSE },
    { "EXIF:Software", "image.software", FALSE },
    { "EXIF:Make", "image.make", FALSE },
    { "EXIF:Model", "image.model", FALSE },
    { "EXIF:DateTimeOriginal", "image.datetime_original", FALSE },
    { "EXIF:GPSLatitude", "image.gps_latitude", TRUE },
    { "EXIF:GPSLongitude", "image.gps_longitude", TRUE },
    { "PDF:Author", "document.author", FALSE },
    { "PDF:Creator", "document.creator", FALSE },
    { "PDF:Producer", "document.producer", FALSE },
    { "PDF:CreateDate", "document.creation_time", FALSE },
    { "PDF:ModifyDate", "document.modification_time", FALSE }
};

static void exiftool_metadata_entry_free(gpointer data)
{
    DocumentMetadataEntry *entry = data;
    if (entry == NULL)
        return;
    g_free(entry->code);
    g_free(entry->original_group);
    g_free(entry->original_tag);
    g_free(entry->raw_value);
    g_free(entry);
}

void exiftool_analysis_result_free(ExiftoolAnalysisResult *result)
{
    if (result == NULL)
        return;
    document_tool_execution_free(result->execution);
    g_ptr_array_unref(result->metadata);
    g_free(result);
}

static const ExiftoolMapping *exiftool_mapping_find(const char *tag)
{
    for (guint index = 0U; index < G_N_ELEMENTS(exiftool_mappings); index++)
        if (g_strcmp0(exiftool_mappings[index].tag, tag) == 0)
            return &exiftool_mappings[index];
    return NULL;
}

static gboolean exiftool_node_depth_valid(JsonNode *node, guint depth)
{
    if (node == NULL || depth > EXIFTOOL_MAX_DEPTH) return FALSE;
    if (JSON_NODE_HOLDS_ARRAY(node)) {
        JsonArray *array = json_node_get_array(node);
        for (guint index = 0U; index < json_array_get_length(array); index++)
            if (!exiftool_node_depth_valid(json_array_get_element(array, index),
                    depth + 1U)) return FALSE;
    } else if (JSON_NODE_HOLDS_OBJECT(node)) {
        JsonObject *object = json_node_get_object(node);
        GList *members = json_object_get_members(object);
        for (GList *item = members; item != NULL; item = item->next)
            if (!exiftool_node_depth_valid(json_object_get_member(object,
                    item->data), depth + 1U)) {
                g_list_free(members); return FALSE;
            }
        g_list_free(members);
    }
    return TRUE;
}

static char *exiftool_node_to_value(JsonNode *node)
{
    if (JSON_NODE_HOLDS_NULL(node)) return g_strdup("null");
    if (JSON_NODE_HOLDS_VALUE(node) &&
        json_node_get_value_type(node) == G_TYPE_STRING)
        return g_strdup(json_node_get_string(node));
    JsonGenerator *generator = json_generator_new();
    json_generator_set_root(generator, node);
    char *value = json_generator_to_data(generator, NULL);
    g_object_unref(generator);
    return value;
}

ExiftoolAnalysisResult *exiftool_analysis_parse(
    const char *file_path,
    const char *json,
    const char *stderr_text,
    int exit_status,
    GError **error
)
{
    if (file_path == NULL || json == NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Le résultat ExifTool à analyser est invalide.");
        return NULL;
    }
    JsonParser *parser = json_parser_new();
    json_parser_set_strict(parser, TRUE);
    GError *parse_error = NULL;
    if (!json_parser_load_from_data(parser, json, -1, &parse_error)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "La sortie JSON ExifTool est invalide ou tronquée : %s",
            parse_error != NULL ? parse_error->message : "syntaxe invalide");
        g_clear_error(&parse_error);
        g_object_unref(parser);
        return NULL;
    }
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root) ||
        json_array_get_length(json_node_get_array(root)) != 1U ||
        !JSON_NODE_HOLDS_OBJECT(json_array_get_element(
            json_node_get_array(root), 0U)) ||
        !exiftool_node_depth_valid(root, 0U)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "La racine, la cardinalité ou la profondeur JSON ExifTool est invalide.");
        g_object_unref(parser);
        return NULL;
    }
    JsonObject *object = json_node_get_object(json_array_get_element(
        json_node_get_array(root), 0U));
    GList *members = json_object_get_members(object);
    if (g_list_length(members) > EXIFTOOL_MAX_TAGS) {
        g_list_free(members); g_object_unref(parser);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
            "La sortie ExifTool dépasse la limite de tags.");
        return NULL;
    }
    ExiftoolAnalysisResult *result = g_new0(ExiftoolAnalysisResult, 1);
    result->execution = document_tool_execution_new("exiftool", file_path);
    result->metadata = g_ptr_array_new_with_free_func(
        exiftool_metadata_entry_free);
    result->execution->raw_stdout = g_strdup(json);
    result->execution->raw_stdout_sha256 = g_compute_checksum_for_string(
        G_CHECKSUM_SHA256, json, -1);
    result->execution->raw_stderr = g_strdup(stderr_text);
    result->execution->exit_status = exit_status;
    result->execution->state = exit_status == 0
        ? DOCUMENT_ANALYSIS_STATE_SUCCESS
        : DOCUMENT_ANALYSIS_STATE_PARTIAL;

    for (GList *item = members; item != NULL; item = item->next) {
        const char *qualified = item->data;
        const char *colon = strchr(qualified, ':');
        const ExiftoolMapping *mapping = exiftool_mapping_find(qualified);
        DocumentMetadataEntry *entry = g_new0(DocumentMetadataEntry, 1);
        entry->code = g_strdup(mapping != NULL
            ? mapping->code : "metadata.unknown");
        entry->original_group = colon != NULL
            ? g_strndup(qualified, (gsize) (colon - qualified))
            : g_strdup("Unknown");
        entry->original_tag = g_strdup(colon != NULL ? colon + 1 : qualified);
        entry->raw_value = exiftool_node_to_value(
            json_object_get_member(object, qualified));
        entry->sensitive = mapping != NULL && mapping->sensitive;
        entry->requires_confirmation = entry->sensitive;
        g_ptr_array_add(result->metadata, entry);
    }
    g_list_free(members);
    g_object_unref(parser);
    return result;
}

ExiftoolAnalysisResult *exiftool_analysis_run(
    const char *executable,
    const char *file_path,
    GCancellable *cancellable,
    GError **error
)
{
    const DocumentToolRunnerLimits limits = {
        DOCUMENT_ANALYSIS_MAX_STDOUT,
        DOCUMENT_ANALYSIS_MAX_STDERR
    };
    return exiftool_analysis_run_with_limits(
        executable, file_path, &limits, cancellable, error);
}

ExiftoolAnalysisResult *exiftool_analysis_run_with_limits(
    const char *executable,
    const char *file_path,
    const DocumentToolRunnerLimits *limits,
    GCancellable *cancellable,
    GError **error
)
{
    const char *arguments[] = {
        "-config", "", "-j", "-G1", "-n", "--", file_path, NULL };
    const char *version_arguments[] = { "-config", "", "-ver", NULL };
    DocumentToolExecution *execution = NULL;
    if (!document_tool_runner_run_with_limits("exiftool", executable,
            arguments, file_path, limits, cancellable, &execution, error))
    {
        document_tool_execution_free(execution);
        return NULL;
    }
    if (execution->state == DOCUMENT_ANALYSIS_STATE_UNAVAILABLE)
    {
        ExiftoolAnalysisResult *unavailable =
            g_new0(ExiftoolAnalysisResult, 1);
        unavailable->execution = execution;
        unavailable->metadata = g_ptr_array_new_with_free_func(
            exiftool_metadata_entry_free);
        return unavailable;
    }
    execution->version = document_tool_runner_read_version(
        executable, version_arguments, cancellable);
    if (execution->stdout_truncated)
    {
        ExiftoolAnalysisResult *truncated =
            g_new0(ExiftoolAnalysisResult, 1);
        truncated->execution = execution;
        truncated->metadata = g_ptr_array_new_with_free_func(
            exiftool_metadata_entry_free);
        execution->state = DOCUMENT_ANALYSIS_STATE_FAILED;
        g_ptr_array_add(execution->errors, g_strdup(
            "Le JSON ExifTool tronqué n'a pas été interprété."));
        return truncated;
    }
    ExiftoolAnalysisResult *result = exiftool_analysis_parse(file_path,
        execution->raw_stdout != NULL ? execution->raw_stdout : "",
        execution->raw_stderr, execution->exit_status, error);
    if (result != NULL)
    {
        document_tool_execution_free(result->execution);
        result->execution = execution;
    }
    else
        document_tool_execution_free(execution);
    return result;
}
