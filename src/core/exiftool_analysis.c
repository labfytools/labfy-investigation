/******************************************************************************
 * @file exiftool_analysis.c
 * @brief Analyse ExifTool structurée et traçable.
 ******************************************************************************/
#include "core/exiftool_analysis.h"
#include "core/document_tool_runner.h"

#include <string.h>

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

static char *exiftool_json_extract_value(
    const char *json,
    const char *tag
)
{
    char *escaped = g_regex_escape_string(tag, -1);
    char *pattern = g_strdup_printf(
        "\"%s\"\\s*:\\s*(\"(?:[^\"\\\\]|\\\\.)*\"|-?[0-9]+(?:\\.[0-9]+)?|true|false|null)",
        escaped
    );
    GRegex *regex = g_regex_new(pattern, G_REGEX_DOTALL, 0, NULL);
    GMatchInfo *match = NULL;
    char *value = NULL;
    g_regex_match(regex, json, 0, &match);
    if (g_match_info_matches(match))
    {
        value = g_match_info_fetch(match, 1);
        if (value[0] == '"' && strlen(value) >= 2)
        {
            gsize length = strlen(value);
            memmove(value, value + 1, length - 2);
            value[length - 2] = '\0';
        }
    }
    g_match_info_free(match);
    g_regex_unref(regex);
    g_free(pattern);
    g_free(escaped);
    return value;
}

static gboolean exiftool_json_shape_is_valid(const char *json)
{
    char *copy = json != NULL ? g_strdup(json) : NULL;
    gboolean valid = FALSE;
    if (copy != NULL)
    {
        g_strstrip(copy);
        gsize length = strlen(copy);
        valid = length >= 2 && copy[0] == '[' && copy[length - 1] == ']';
    }
    g_free(copy);
    return valid;
}

static gboolean exiftool_metadata_contains_tag(
    const GPtrArray *metadata,
    const char *group,
    const char *tag
)
{
    for (guint index = 0; index < metadata->len; index++)
    {
        const DocumentMetadataEntry *entry =
            g_ptr_array_index((GPtrArray *) metadata, index);
        if (g_strcmp0(entry->original_group, group) == 0 &&
            g_strcmp0(entry->original_tag, tag) == 0)
            return TRUE;
    }
    return FALSE;
}

static void exiftool_analysis_add_unknown_tags(
    ExiftoolAnalysisResult *result,
    const char *json
)
{
    GRegex *regex = g_regex_new(
        "\"([A-Za-z0-9_ -]+):([A-Za-z0-9_ -]+)\"\\s*:\\s*"
        "(\"(?:[^\"\\\\]|\\\\.)*\"|-?[0-9]+(?:\\.[0-9]+)?|true|false|null)",
        G_REGEX_DOTALL, 0, NULL);
    GMatchInfo *match = NULL;
    g_regex_match(regex, json, 0, &match);
    while (g_match_info_matches(match))
    {
        char *group = g_match_info_fetch(match, 1);
        char *tag = g_match_info_fetch(match, 2);
        char *value = g_match_info_fetch(match, 3);
        if (!exiftool_metadata_contains_tag(result->metadata, group, tag))
        {
            DocumentMetadataEntry *entry =
                g_new0(DocumentMetadataEntry, 1);
            entry->code = g_strdup("metadata.unknown");
            entry->original_group = group;
            entry->original_tag = tag;
            entry->raw_value = value;
            g_ptr_array_add(result->metadata, entry);
        }
        else
        {
            g_free(group);
            g_free(tag);
            g_free(value);
        }
        if (!g_match_info_next(match, NULL))
            break;
    }
    g_match_info_free(match);
    g_regex_unref(regex);
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
    if (!exiftool_json_shape_is_valid(json))
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "La sortie JSON ExifTool est invalide ou tronquée.");
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

    for (guint index = 0; index < G_N_ELEMENTS(exiftool_mappings); index++)
    {
        char *value = exiftool_json_extract_value(
            json, exiftool_mappings[index].tag);
        if (value == NULL)
            continue;
        DocumentMetadataEntry *entry = g_new0(DocumentMetadataEntry, 1);
        entry->code = g_strdup(exiftool_mappings[index].code);
        const char *colon = strchr(exiftool_mappings[index].tag, ':');
        entry->original_group = g_strndup(exiftool_mappings[index].tag,
            (gsize) (colon - exiftool_mappings[index].tag));
        entry->original_tag = g_strdup(colon + 1);
        entry->raw_value = value;
        entry->sensitive = exiftool_mappings[index].sensitive;
        entry->requires_confirmation = entry->sensitive;
        g_ptr_array_add(result->metadata, entry);
    }
    exiftool_analysis_add_unknown_tags(result, json);
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
    const char *arguments[] = { "-j", "-G1", "-n", "--", file_path, NULL };
    const char *version_arguments[] = { "-ver", NULL };
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
