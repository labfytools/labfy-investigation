/******************************************************************************
 * @file eml_mime_extractor.c
 * @brief Extraction MIME récursive, bornée et non destructive.
 ******************************************************************************/
#include "core/eml_mime_extractor.h"
#include "core/file_hash.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>

#define EML_MIME_MAX_COLLISIONS 10000U
#define EML_MIME_IO_BLOCK_SIZE 65536U

typedef struct
{
    GHashTable *values;
} EmlMimeHeaders;

typedef struct
{
    const char *target_dir;
    const char *target_canonical;
    GCancellable *cancellable;
    EmlMimeResult *result;
    guint part_count;
    gsize total_decoded_size;
} EmlMimeContext;

static gboolean eml_mime_check_cancelled(
    GCancellable *cancellable,
    GError **error
)
{
    return cancellable != NULL &&
        g_cancellable_set_error_if_cancelled(cancellable, error);
}

void eml_attachment_free(EmlAttachment *attachment)
{
    if (attachment == NULL)
        return;
    g_free(attachment->part_index);
    g_free(attachment->declared_filename);
    g_free(attachment->decoded_filename);
    g_free(attachment->sanitized_filename);
    g_free(attachment->extracted_path);
    g_free(attachment->relative_path);
    g_free(attachment->content_type);
    g_free(attachment->detected_mime);
    g_free(attachment->content_id);
    g_free(attachment->normalized_content_id);
    g_free(attachment->content_disposition);
    g_free(attachment->normalized_disposition);
    g_free(attachment->transfer_encoding);
    g_free(attachment->extracted_at_utc);
    g_free(attachment->sha256);
    g_free(attachment);
}

void eml_mime_result_free(EmlMimeResult *result)
{
    if (result == NULL)
        return;
    g_clear_pointer(&result->attachments, g_ptr_array_unref);
    g_clear_pointer(&result->warnings, g_ptr_array_unref);
    g_free(result);
}

static void eml_mime_warn(
    EmlMimeContext *context,
    const char *part_index,
    const char *message
)
{
    g_ptr_array_add(
        context->result->warnings,
        g_strdup_printf(
            "Partie MIME %s : %s",
            part_index != NULL ? part_index : "1",
            message
        )
    );
}

static EmlMimeHeaders *eml_mime_headers_new(void)
{
    EmlMimeHeaders *headers = g_new0(EmlMimeHeaders, 1);
    headers->values = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        g_free,
        g_free
    );
    return headers;
}

static void eml_mime_headers_free(EmlMimeHeaders *headers)
{
    if (headers == NULL)
        return;
    g_hash_table_unref(headers->values);
    g_free(headers);
}

static const char *eml_mime_headers_get(
    const EmlMimeHeaders *headers,
    const char *name
)
{
    char *key = NULL;
    const char *value = NULL;

    if (headers == NULL || name == NULL)
        return NULL;
    key = g_ascii_strdown(name, -1);
    value = g_hash_table_lookup(headers->values, key);
    g_free(key);
    return value;
}

static gboolean eml_mime_headers_store(
    EmlMimeHeaders *headers,
    const char *name,
    const char *value,
    GError **error
)
{
    char *key = NULL;
    char *safe_value = NULL;

    if (strlen(value) > EML_MIME_MAX_HEADER_VALUE_LENGTH)
    {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_DATA,
            "Une valeur d'en-tête MIME dépasse la limite autorisée."
        );
        return FALSE;
    }
    key = g_ascii_strdown(name, -1);
    safe_value = g_utf8_make_valid(value, -1);
    g_strstrip(key);
    g_strstrip(safe_value);
    g_hash_table_replace(headers->values, key, safe_value);
    return TRUE;
}

static EmlMimeHeaders *eml_mime_parse_headers(
    const char *data,
    gsize length,
    gsize *body_offset,
    GError **error
)
{
    EmlMimeHeaders *headers = eml_mime_headers_new();
    GString *current_value = g_string_new(NULL);
    char *current_name = NULL;
    gsize cursor = 0;
    gboolean separator_found = FALSE;

    while (cursor < length)
    {
        gsize line_start = cursor;
        gsize line_length = 0;
        char *line = NULL;

        while (cursor < length && data[cursor] != '\n')
            cursor++;
        line_length = cursor - line_start;
        if (cursor < length)
            cursor++;
        if (line_length > 0 && data[line_start + line_length - 1] == '\r')
            line_length--;

        if (line_length == 0)
        {
            separator_found = TRUE;
            break;
        }
        if (line_length > EML_MIME_MAX_HEADER_VALUE_LENGTH)
        {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_INVALID_DATA,
                "Une ligne d'en-tête MIME dépasse la limite autorisée."
            );
            goto failure;
        }

        line = g_strndup(data + line_start, line_length);
        if ((line[0] == ' ' || line[0] == '\t') && current_name != NULL)
        {
            g_string_append_c(current_value, ' ');
            g_string_append(current_value, g_strstrip(line));
            g_free(line);
            continue;
        }

        if (current_name != NULL &&
            !eml_mime_headers_store(
                headers,
                current_name,
                current_value->str,
                error
            ))
        {
            g_free(line);
            goto failure;
        }
        g_clear_pointer(&current_name, g_free);
        g_string_truncate(current_value, 0);

        char *colon = strchr(line, ':');
        if (colon == NULL || colon == line)
        {
            g_free(line);
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_INVALID_DATA,
                "Une ligne d'en-tête MIME est malformée."
            );
            goto failure;
        }
        current_name = g_strndup(line, (gsize) (colon - line));
        g_string_assign(current_value, g_strstrip(colon + 1));
        g_free(line);
    }

    if (current_name != NULL &&
        !eml_mime_headers_store(
            headers,
            current_name,
            current_value->str,
            error
        ))
        goto failure;

    g_free(current_name);
    g_string_free(current_value, TRUE);
    *body_offset = cursor;
    if (!separator_found)
        *body_offset = length;
    return headers;

failure:
    g_free(current_name);
    g_string_free(current_value, TRUE);
    eml_mime_headers_free(headers);
    return NULL;
}

static GHashTable *eml_mime_parse_parameters(
    const char *header_value,
    char **main_value,
    gboolean *malformed
)
{
    GHashTable *parameters = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        g_free,
        g_free
    );
    const char *cursor = header_value != NULL ? header_value : "";
    const char *semicolon = strchr(cursor, ';');

    *malformed = FALSE;
    *main_value = semicolon != NULL
        ? g_strndup(cursor, (gsize) (semicolon - cursor))
        : g_strdup(cursor);
    g_strstrip(*main_value);
    cursor = semicolon != NULL ? semicolon + 1 : cursor + strlen(cursor);

    while (*cursor != '\0')
    {
        const char *name_start = NULL;
        const char *value_start = NULL;
        char *name = NULL;
        char *value = NULL;

        while (g_ascii_isspace(*cursor) || *cursor == ';')
            cursor++;
        if (*cursor == '\0')
            break;
        name_start = cursor;
        while (*cursor != '\0' && *cursor != '=' && *cursor != ';')
            cursor++;
        if (*cursor != '=')
        {
            *malformed = TRUE;
            break;
        }
        name = g_strndup(name_start, (gsize) (cursor - name_start));
        g_strstrip(name);
        char *lower_name = g_ascii_strdown(name, -1);
        g_free(name);
        cursor++;
        while (g_ascii_isspace(*cursor))
            cursor++;
        value_start = cursor;
        if (*cursor == '"')
        {
            GString *quoted = g_string_new(NULL);
            gboolean closed = FALSE;
            cursor++;
            while (*cursor != '\0')
            {
                if (*cursor == '\\' && cursor[1] != '\0')
                {
                    cursor++;
                    g_string_append_c(quoted, *cursor++);
                }
                else if (*cursor == '"')
                {
                    cursor++;
                    closed = TRUE;
                    break;
                }
                else
                    g_string_append_c(quoted, *cursor++);
            }
            value = g_string_free(quoted, FALSE);
            if (!closed)
                *malformed = TRUE;
        }
        else
        {
            while (*cursor != '\0' && *cursor != ';')
                cursor++;
            value = g_strndup(value_start, (gsize) (cursor - value_start));
            g_strstrip(value);
        }

        if (g_hash_table_contains(parameters, lower_name))
        {
            *malformed = TRUE;
            g_hash_table_remove(parameters, lower_name);
            g_free(lower_name);
            g_free(value);
        }
        else
            g_hash_table_insert(parameters, lower_name, value);

        while (g_ascii_isspace(*cursor))
            cursor++;
        if (*cursor != '\0' && *cursor != ';')
            *malformed = TRUE;
    }
    return parameters;
}

static char *eml_mime_convert_charset(
    const guint8 *data,
    gsize length,
    const char *charset
)
{
    GError *error = NULL;
    char *converted = NULL;

    if (charset == NULL ||
        charset[0] == '\0' ||
        g_ascii_strcasecmp(charset, "UTF-8") == 0 ||
        g_ascii_strcasecmp(charset, "US-ASCII") == 0)
        return g_utf8_make_valid((const char *) data, (gssize) length);

    converted = g_convert(
        (const char *) data,
        (gssize) length,
        "UTF-8",
        charset,
        NULL,
        NULL,
        &error
    );
    g_clear_error(&error);
    return converted;
}

static GBytes *eml_mime_percent_decode(const char *value)
{
    GByteArray *decoded = g_byte_array_new();

    for (gsize index = 0; value[index] != '\0'; index++)
    {
        guint8 byte = (guint8) value[index];
        if (byte == '%')
        {
            if (!g_ascii_isxdigit(value[index + 1]) ||
                !g_ascii_isxdigit(value[index + 2]))
            {
                g_byte_array_unref(decoded);
                return NULL;
            }
            char hex[3] = { value[index + 1], value[index + 2], '\0' };
            byte = (guint8) g_ascii_strtoull(hex, NULL, 16);
            index += 2;
        }
        g_byte_array_append(decoded, &byte, 1);
    }
    return g_byte_array_free_to_bytes(decoded);
}

static char *eml_mime_decode_extended_value(const char *value)
{
    const char *first_quote = strchr(value, '\'');
    const char *second_quote = first_quote != NULL
        ? strchr(first_quote + 1, '\'')
        : NULL;
    char *charset = NULL;
    GBytes *bytes = NULL;
    gconstpointer data = NULL;
    gsize length = 0;
    char *result = NULL;

    if (first_quote == NULL || second_quote == NULL)
        return NULL;
    charset = g_strndup(value, (gsize) (first_quote - value));
    bytes = eml_mime_percent_decode(second_quote + 1);
    if (bytes != NULL)
    {
        data = g_bytes_get_data(bytes, &length);
        result = eml_mime_convert_charset(data, length, charset);
        g_bytes_unref(bytes);
    }
    g_free(charset);
    return result;
}

static char *eml_mime_decode_rfc2047_word(
    const char *start,
    const char **next
)
{
    const char *charset_end = strstr(start + 2, "?");
    const char *encoding_end = charset_end != NULL
        ? strstr(charset_end + 1, "?")
        : NULL;
    const char *word_end = encoding_end != NULL
        ? strstr(encoding_end + 1, "?=")
        : NULL;
    char *charset = NULL;
    char encoding = '\0';
    GByteArray *decoded = NULL;
    char *result = NULL;

    if (charset_end == NULL || encoding_end == NULL || word_end == NULL ||
        encoding_end != charset_end + 2)
        return NULL;
    charset = g_strndup(start + 2, (gsize) (charset_end - start - 2));
    encoding = g_ascii_toupper(charset_end[1]);
    decoded = g_byte_array_new();

    if (encoding == 'B')
    {
        char *payload = g_strndup(
            encoding_end + 1,
            (gsize) (word_end - encoding_end - 1)
        );
        gsize output_length = 0;
        guchar *output = g_base64_decode(payload, &output_length);
        g_free(payload);
        if (output == NULL)
            goto cleanup;
        g_byte_array_append(decoded, output, (guint) output_length);
        g_free(output);
    }
    else if (encoding == 'Q')
    {
        for (const char *cursor = encoding_end + 1;
             cursor < word_end;
             cursor++)
        {
            guint8 byte = (guint8) *cursor;
            if (byte == '_')
                byte = ' ';
            else if (byte == '=')
            {
                if (cursor + 2 >= word_end ||
                    !g_ascii_isxdigit(cursor[1]) ||
                    !g_ascii_isxdigit(cursor[2]))
                    goto cleanup;
                char hex[3] = { cursor[1], cursor[2], '\0' };
                byte = (guint8) g_ascii_strtoull(hex, NULL, 16);
                cursor += 2;
            }
            g_byte_array_append(decoded, &byte, 1);
        }
    }
    else
        goto cleanup;

    result = eml_mime_convert_charset(decoded->data, decoded->len, charset);
    if (result != NULL)
        *next = word_end + 2;

cleanup:
    g_byte_array_unref(decoded);
    g_free(charset);
    return result;
}

static char *eml_mime_decode_rfc2047(const char *value)
{
    GString *result = g_string_new(NULL);
    const char *cursor = value;

    while (*cursor != '\0')
    {
        if (cursor[0] == '=' && cursor[1] == '?')
        {
            const char *next = NULL;
            char *decoded = eml_mime_decode_rfc2047_word(cursor, &next);
            if (decoded != NULL)
            {
                g_string_append(result, decoded);
                g_free(decoded);
                cursor = next;
                while (g_ascii_isspace(*cursor) &&
                       cursor[1] == '=' &&
                       cursor[2] == '?')
                    cursor++;
                continue;
            }
        }
        g_string_append_c(result, *cursor++);
    }
    return g_string_free(result, FALSE);
}

static char *eml_mime_resolve_continuation(
    GHashTable *parameters,
    const char *base_name
)
{
    GString *joined = g_string_new(NULL);
    gboolean encoded_first = FALSE;
    gboolean found = FALSE;
    char *charset = NULL;

    for (guint index = 0; index < EML_MIME_MAX_PARTS; index++)
    {
        char *encoded_key = g_strdup_printf("%s*%u*", base_name, index);
        char *plain_key = g_strdup_printf("%s*%u", base_name, index);
        const char *segment = g_hash_table_lookup(parameters, encoded_key);
        gboolean encoded = segment != NULL;

        if (segment == NULL)
            segment = g_hash_table_lookup(parameters, plain_key);
        g_free(encoded_key);
        g_free(plain_key);

        if (segment == NULL)
        {
            if (index == 0)
            {
                g_string_free(joined, TRUE);
                return NULL;
            }
            GHashTableIter iterator;
            gpointer key = NULL;
            gboolean later_segment = FALSE;
            char *prefix = g_strdup_printf("%s*", base_name);
            g_hash_table_iter_init(&iterator, parameters);
            while (g_hash_table_iter_next(&iterator, &key, NULL))
            {
                const char *parameter_name = key;
                if (g_str_has_prefix(parameter_name, prefix) &&
                    g_ascii_isdigit(parameter_name[strlen(prefix)]))
                {
                    guint parameter_index = (guint) g_ascii_strtoull(
                        parameter_name + strlen(prefix),
                        NULL,
                        10
                    );
                    if (parameter_index > index)
                        later_segment = TRUE;
                }
            }
            g_free(prefix);
            if (later_segment)
            {
                g_free(charset);
                g_string_free(joined, TRUE);
                return NULL;
            }
            break;
        }
        if (index == 0)
            encoded_first = encoded;
        if (encoded)
        {
            GBytes *bytes = NULL;
            const char *payload = segment;
            if (index == 0 && encoded_first)
            {
                const char *first_quote = strchr(segment, '\'');
                const char *second_quote = first_quote != NULL
                    ? strchr(first_quote + 1, '\'')
                    : NULL;
                if (second_quote == NULL)
                {
                    g_free(charset);
                    g_string_free(joined, TRUE);
                    return NULL;
                }
                charset = g_strndup(
                    segment,
                    (gsize) (first_quote - segment)
                );
                payload = second_quote + 1;
            }
            bytes = eml_mime_percent_decode(payload);
            if (bytes == NULL)
            {
                g_free(charset);
                g_string_free(joined, TRUE);
                return NULL;
            }
            gsize length = 0;
            const char *data = g_bytes_get_data(bytes, &length);
            g_string_append_len(joined, data, (gssize) length);
            g_bytes_unref(bytes);
        }
        else
            g_string_append(joined, segment);
        found = TRUE;
    }

    if (!found)
    {
        g_free(charset);
        g_string_free(joined, TRUE);
        return NULL;
    }
    char *result = eml_mime_convert_charset(
        (const guint8 *) joined->str,
        joined->len,
        charset
    );
    g_free(charset);
    g_string_free(joined, TRUE);
    return result;
}

static char *eml_mime_resolve_parameter(
    GHashTable *parameters,
    const char *base_name
)
{
    char *value = eml_mime_resolve_continuation(parameters, base_name);
    char *extended_key = NULL;
    const char *raw_value = NULL;

    if (value != NULL)
        return value;
    extended_key = g_strdup_printf("%s*", base_name);
    raw_value = g_hash_table_lookup(parameters, extended_key);
    g_free(extended_key);
    if (raw_value != NULL)
    {
        value = eml_mime_decode_extended_value(raw_value);
        if (value != NULL)
            return value;
    }
    raw_value = g_hash_table_lookup(parameters, base_name);
    return raw_value != NULL ? eml_mime_decode_rfc2047(raw_value) : NULL;
}

static char *eml_mime_raw_parameter(
    GHashTable *parameters,
    const char *base_name
)
{
    GString *continuation = g_string_new(NULL);
    gboolean found = FALSE;

    for (guint index = 0; index < EML_MIME_MAX_PARTS; index++)
    {
        char *encoded_key = g_strdup_printf("%s*%u*", base_name, index);
        char *plain_key = g_strdup_printf("%s*%u", base_name, index);
        const char *segment = g_hash_table_lookup(parameters, encoded_key);
        if (segment == NULL)
            segment = g_hash_table_lookup(parameters, plain_key);
        g_free(encoded_key);
        g_free(plain_key);
        if (segment == NULL)
            break;
        g_string_append(continuation, segment);
        found = TRUE;
    }
    if (found)
        return g_string_free(continuation, FALSE);
    g_string_free(continuation, TRUE);

    char *extended_key = g_strdup_printf("%s*", base_name);
    const char *raw = g_hash_table_lookup(parameters, extended_key);
    g_free(extended_key);
    if (raw == NULL)
        raw = g_hash_table_lookup(parameters, base_name);
    return g_strdup(raw);
}

char *eml_mime_sanitize_filename(const char *raw_filename)
{
    char *valid = raw_filename != NULL
        ? g_utf8_make_valid(raw_filename, -1)
        : NULL;
    GString *sanitized = g_string_new(NULL);

    if (valid != NULL)
    {
        for (const char *cursor = valid;
             *cursor != '\0' &&
             sanitized->len < EML_MIME_MAX_FILENAME_LENGTH;
             cursor = g_utf8_next_char(cursor))
        {
            gunichar character = g_utf8_get_char(cursor);
            if (character < 0x20 ||
                character == 0x7f ||
                character == '/' ||
                character == '\\' ||
                character == ':' ||
                character == '*' ||
                character == '?' ||
                character == '"' ||
                character == '<' ||
                character == '>' ||
                character == '|')
                g_string_append_c(sanitized, '_');
            else
            {
                char utf8[6] = { 0 };
                gint width = g_unichar_to_utf8(character, utf8);
                if (sanitized->len + (gsize) width >
                    EML_MIME_MAX_FILENAME_LENGTH)
                    break;
                g_string_append_len(sanitized, utf8, width);
            }
        }
    }
    g_free(valid);

    while (strstr(sanitized->str, "..") != NULL)
    {
        char *dots = strstr(sanitized->str, "..");
        dots[0] = '_';
        dots[1] = '_';
    }
    g_strstrip(sanitized->str);
    g_string_set_size(sanitized, strlen(sanitized->str));
    while (sanitized->len > 0 &&
           (sanitized->str[sanitized->len - 1] == '.' ||
            sanitized->str[sanitized->len - 1] == ' '))
        g_string_truncate(sanitized, sanitized->len - 1);

    if (sanitized->len == 0 ||
        g_str_equal(sanitized->str, ".") ||
        g_str_equal(sanitized->str, ".."))
        g_string_assign(sanitized, "attachment.bin");
    return g_string_free(sanitized, FALSE);
}

static GBytes *eml_mime_decode_transfer(
    EmlMimeContext *context,
    const char *encoding,
    const char *data,
    gsize length,
    GError **error
)
{
    GByteArray *decoded = g_byte_array_new();
    char *normalized = encoding != NULL
        ? g_ascii_strdown(encoding, -1)
        : g_strdup("7bit");

    if (g_str_equal(normalized, "base64"))
    {
        GString *compact = g_string_sized_new(length);
        guint padding = 0;
        for (gsize index = 0; index < length; index++)
        {
            if ((index % EML_MIME_IO_BLOCK_SIZE) == 0 &&
                eml_mime_check_cancelled(context->cancellable, error))
                goto failure;
            if (g_ascii_isspace(data[index]))
                continue;
            if (data[index] == '=')
                padding++;
            else if (!g_ascii_isalnum(data[index]) &&
                     data[index] != '+' &&
                     data[index] != '/')
            {
                g_set_error_literal(
                    error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_DATA,
                    "Le contenu Base64 contient un caractère invalide."
                );
                g_string_free(compact, TRUE);
                goto failure;
            }
            else if (padding > 0)
            {
                g_set_error_literal(
                    error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_DATA,
                    "Le padding Base64 est invalide."
                );
                g_string_free(compact, TRUE);
                goto failure;
            }
            g_string_append_c(compact, data[index]);
        }
        if (compact->len > 0 &&
            (compact->len % 4 != 0 || padding > 2))
        {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_INVALID_DATA,
                "Le contenu Base64 est tronqué ou son padding est invalide."
            );
            g_string_free(compact, TRUE);
            goto failure;
        }
        if (compact->len > 0)
        {
            gsize output_length = 0;
            guchar *output = g_base64_decode(
                compact->str,
                &output_length
            );
            if (output_length > EML_MIME_MAX_PART_DECODED_SIZE)
            {
                g_free(output);
                g_string_free(compact, TRUE);
                g_set_error_literal(
                    error,
                    G_IO_ERROR,
                    G_IO_ERROR_NO_SPACE,
                    "La taille décodée de la partie dépasse la limite."
                );
                goto failure;
            }
            g_byte_array_append(decoded, output, (guint) output_length);
            g_free(output);
        }
        g_string_free(compact, TRUE);
    }
    else if (g_str_equal(normalized, "quoted-printable"))
    {
        for (gsize index = 0; index < length; index++)
        {
            guint8 byte = (guint8) data[index];
            if ((index % EML_MIME_IO_BLOCK_SIZE) == 0 &&
                eml_mime_check_cancelled(context->cancellable, error))
                goto failure;
            if (byte == '=')
            {
                if (index + 1 < length && data[index + 1] == '\n')
                {
                    index++;
                    continue;
                }
                if (index + 2 < length &&
                    data[index + 1] == '\r' &&
                    data[index + 2] == '\n')
                {
                    index += 2;
                    continue;
                }
                if (index + 2 >= length ||
                    !g_ascii_isxdigit(data[index + 1]) ||
                    !g_ascii_isxdigit(data[index + 2]))
                {
                    g_set_error_literal(
                        error,
                        G_IO_ERROR,
                        G_IO_ERROR_INVALID_DATA,
                        "Une séquence quoted-printable est invalide."
                    );
                    goto failure;
                }
                char hex[3] = {
                    data[index + 1],
                    data[index + 2],
                    '\0'
                };
                byte = (guint8) g_ascii_strtoull(hex, NULL, 16);
                index += 2;
            }
            g_byte_array_append(decoded, &byte, 1);
            if (decoded->len > EML_MIME_MAX_PART_DECODED_SIZE)
            {
                g_set_error_literal(
                    error,
                    G_IO_ERROR,
                    G_IO_ERROR_NO_SPACE,
                    "La taille décodée de la partie dépasse la limite."
                );
                goto failure;
            }
        }
    }
    else if (g_str_equal(normalized, "7bit") ||
             g_str_equal(normalized, "8bit") ||
             g_str_equal(normalized, "binary"))
    {
        if (length > EML_MIME_MAX_PART_DECODED_SIZE)
        {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_NO_SPACE,
                "La taille décodée de la partie dépasse la limite."
            );
            goto failure;
        }
        g_byte_array_append(decoded, (const guint8 *) data, (guint) length);
    }
    else
    {
        g_set_error(
            error,
            G_IO_ERROR,
            G_IO_ERROR_NOT_SUPPORTED,
            "L'encodage de transfert « %s » n'est pas pris en charge.",
            encoding
        );
        goto failure;
    }

    g_free(normalized);
    return g_byte_array_free_to_bytes(decoded);

failure:
    g_free(normalized);
    g_byte_array_unref(decoded);
    return NULL;
}

static const char *eml_mime_extension_for_type(const char *content_type)
{
    if (g_strcmp0(content_type, "text/plain") == 0)
        return "txt";
    if (g_strcmp0(content_type, "text/html") == 0)
        return "html";
    if (g_strcmp0(content_type, "image/png") == 0)
        return "png";
    if (g_strcmp0(content_type, "image/jpeg") == 0)
        return "jpg";
    if (g_strcmp0(content_type, "application/pdf") == 0)
        return "pdf";
    if (g_strcmp0(content_type, "message/rfc822") == 0)
        return "eml";
    return "bin";
}

static gboolean eml_mime_name_exists_casefold(
    const char *directory,
    const char *name
)
{
    GDir *dir = g_dir_open(directory, 0, NULL);
    const char *entry = NULL;
    gboolean exists = FALSE;

    if (dir == NULL)
    {
        char *path = g_build_filename(directory, name, NULL);
        gboolean exists = g_file_test(path, G_FILE_TEST_EXISTS);
        g_free(path);
        return exists;
    }
    while ((entry = g_dir_read_name(dir)) != NULL)
    {
        if (g_ascii_strcasecmp(entry, name) == 0)
        {
            exists = TRUE;
            break;
        }
    }
    g_dir_close(dir);
    return exists;
}

static char *eml_mime_unique_name(
    const char *directory,
    const char *sanitized,
    GError **error
)
{
    char *stem = NULL;
    char *extension = NULL;
    const char *dot = strrchr(sanitized, '.');

    if (dot != NULL && dot != sanitized)
    {
        stem = g_strndup(sanitized, (gsize) (dot - sanitized));
        extension = g_strdup(dot);
    }
    else
    {
        stem = g_strdup(sanitized);
        extension = g_strdup("");
    }

    for (guint index = 1; index <= EML_MIME_MAX_COLLISIONS; index++)
    {
        char *candidate = index == 1
            ? g_strdup(sanitized)
            : g_strdup_printf("%s-%u%s", stem, index, extension);
        if (!eml_mime_name_exists_casefold(directory, candidate))
        {
            g_free(stem);
            g_free(extension);
            return candidate;
        }
        g_free(candidate);
    }
    g_free(stem);
    g_free(extension);
    g_set_error_literal(
        error,
        G_IO_ERROR,
        G_IO_ERROR_EXISTS,
        "Impossible de produire un nom de fichier dérivé unique."
    );
    return NULL;
}

static gboolean eml_mime_write_atomic(
    EmlMimeContext *context,
    const char *final_path,
    GBytes *bytes,
    GError **error
)
{
    char *template = g_build_filename(
        context->target_dir,
        ".eml-mime-XXXXXX",
        NULL
    );
    gint descriptor = g_mkstemp(template);
    GFile *temporary_file = NULL;
    GFile *final_file = NULL;
    gboolean success = FALSE;

    if (descriptor < 0)
    {
        g_set_error(
            error,
            G_FILE_ERROR,
            g_file_error_from_errno(errno),
            "Impossible de créer le fichier temporaire : %s",
            g_strerror(errno)
        );
        g_free(template);
        return FALSE;
    }
    gsize length = 0;
    const guint8 *data = g_bytes_get_data(bytes, &length);
    gsize offset = 0;

    while (offset < length)
    {
        gsize block = MIN(
            (gsize) EML_MIME_IO_BLOCK_SIZE,
            length - offset
        );
        if (eml_mime_check_cancelled(context->cancellable, error))
            goto cleanup;
        ssize_t written = write(descriptor, data + offset, block);
        if (written < 0)
        {
            g_set_error(
                error,
                G_FILE_ERROR,
                g_file_error_from_errno(errno),
                "Échec d'écriture du fichier temporaire : %s",
                g_strerror(errno)
            );
            goto cleanup;
        }
        offset += (gsize) written;
    }
    if (close(descriptor) != 0)
    {
        descriptor = -1;
        g_set_error(
            error,
            G_FILE_ERROR,
            g_file_error_from_errno(errno),
            "Échec de fermeture du fichier temporaire : %s",
            g_strerror(errno)
        );
        goto cleanup;
    }
    descriptor = -1;
    if (eml_mime_check_cancelled(context->cancellable, error))
        goto cleanup;

    temporary_file = g_file_new_for_path(template);
    final_file = g_file_new_for_path(final_path);
    if (!g_file_move(
            temporary_file,
            final_file,
            G_FILE_COPY_NONE,
            context->cancellable,
            NULL,
            NULL,
            error
        ))
        goto cleanup;
    success = TRUE;

cleanup:
    if (descriptor >= 0)
        close(descriptor);
    if (!success)
        g_remove(template);
    g_clear_object(&temporary_file);
    g_clear_object(&final_file);
    g_free(template);
    return success;
}

static char *eml_mime_normalize_content_id(const char *content_id)
{
    char *normalized = content_id != NULL ? g_strdup(content_id) : NULL;
    if (normalized == NULL)
        return NULL;
    g_strstrip(normalized);
    gsize length = strlen(normalized);
    if (length >= 2 && normalized[0] == '<' &&
        normalized[length - 1] == '>')
    {
        normalized[length - 1] = '\0';
        memmove(normalized, normalized + 1, length - 1);
    }
    return normalized;
}

static gboolean eml_mime_extract_leaf(
    EmlMimeContext *context,
    const EmlMimeHeaders *headers,
    const char *body,
    gsize body_length,
    const char *part_index,
    const char *content_type,
    GHashTable *type_parameters,
    GError **error
)
{
    const char *raw_disposition = eml_mime_headers_get(
        headers,
        "content-disposition"
    );
    const char *encoding = eml_mime_headers_get(
        headers,
        "content-transfer-encoding"
    );
    const char *content_id = eml_mime_headers_get(headers, "content-id");
    char *disposition = NULL;
    gboolean disposition_malformed = FALSE;
    GHashTable *disposition_parameters = eml_mime_parse_parameters(
        raw_disposition,
        &disposition,
        &disposition_malformed
    );
    char *filename = eml_mime_resolve_parameter(
        disposition_parameters,
        "filename"
    );
    char *raw_filename = eml_mime_raw_parameter(
        disposition_parameters,
        "filename"
    );
    char *type_name = NULL;
    gboolean relevant = FALSE;
    GBytes *decoded = NULL;
    GError *decode_error = NULL;

    if (filename == NULL)
    {
        type_name = eml_mime_resolve_parameter(type_parameters, "name");
        g_clear_pointer(&raw_filename, g_free);
        raw_filename = eml_mime_raw_parameter(type_parameters, "name");
    }
    if (filename == NULL)
        filename = g_steal_pointer(&type_name);
    relevant = filename != NULL ||
        content_id != NULL ||
        g_ascii_strcasecmp(disposition, "attachment") == 0 ||
        g_ascii_strcasecmp(disposition, "inline") == 0;

    if (!relevant)
        goto cleanup;
    if (disposition_malformed)
        eml_mime_warn(
            context,
            part_index,
            "Content-Disposition contient un paramètre malformé."
        );

    decoded = eml_mime_decode_transfer(
        context,
        encoding,
        body,
        body_length,
        &decode_error
    );
    if (decoded == NULL)
    {
        if (g_error_matches(
                decode_error,
                G_IO_ERROR,
                G_IO_ERROR_CANCELLED
            ))
        {
            g_propagate_error(error, decode_error);
            g_hash_table_unref(disposition_parameters);
            g_free(disposition);
            g_free(filename);
            g_free(raw_filename);
            return FALSE;
        }
        eml_mime_warn(context, part_index, decode_error->message);
        g_clear_error(&decode_error);
        goto cleanup;
    }

    gsize decoded_length = 0;
    gconstpointer decoded_data = g_bytes_get_data(decoded, &decoded_length);
    if (context->total_decoded_size >
        EML_MIME_MAX_TOTAL_DECODED_SIZE - decoded_length)
    {
        eml_mime_warn(
            context,
            part_index,
            "la taille décodée cumulée maximale est dépassée."
        );
        goto cleanup;
    }

    char *synthetic = NULL;
    if (filename == NULL || filename[0] == '\0')
        synthetic = g_strdup_printf(
            "part-%s.%s",
            part_index,
            eml_mime_extension_for_type(content_type)
        );
    const char *decoded_name = filename != NULL ? filename : synthetic;
    const char *declared = raw_filename != NULL
        ? raw_filename
        : decoded_name;
    char *sanitized = eml_mime_sanitize_filename(decoded_name);
    char *unique = eml_mime_unique_name(
        context->target_dir,
        sanitized,
        error
    );
    char *final_path = unique != NULL
        ? g_build_filename(context->target_dir, unique, NULL)
        : NULL;
    char *canonical = final_path != NULL
        ? g_canonicalize_filename(final_path, NULL)
        : NULL;

    if (unique == NULL ||
        !g_str_has_prefix(canonical, context->target_canonical) ||
        canonical[strlen(context->target_canonical)] != G_DIR_SEPARATOR)
    {
        if (unique != NULL)
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_PERMISSION_DENIED,
                "Le chemin final sortirait du répertoire de destination."
            );
        g_free(canonical);
        g_free(final_path);
        g_free(unique);
        g_free(sanitized);
        g_free(synthetic);
        goto fatal;
    }

    if (!eml_mime_write_atomic(context, final_path, decoded, error))
    {
        g_free(canonical);
        g_free(final_path);
        g_free(unique);
        g_free(sanitized);
        g_free(synthetic);
        goto fatal;
    }

    EmlAttachment *attachment = g_new0(EmlAttachment, 1);
    attachment->part_index = g_strdup(part_index);
    attachment->declared_filename = g_strdup(declared);
    attachment->decoded_filename = g_strdup(decoded_name);
    attachment->sanitized_filename = unique;
    attachment->extracted_path = final_path;
    attachment->relative_path = g_strdup(unique);
    attachment->content_type = g_strdup(content_type);
    attachment->detected_mime = g_content_type_guess(
        final_path,
        decoded_data,
        decoded_length,
        NULL
    );
    attachment->content_id = g_strdup(content_id);
    attachment->normalized_content_id =
        eml_mime_normalize_content_id(content_id);
    attachment->content_disposition = g_strdup(raw_disposition);
    attachment->normalized_disposition =
        disposition[0] != '\0' ? g_ascii_strdown(disposition, -1) : NULL;
    attachment->transfer_encoding = g_strdup(
        encoding != NULL ? encoding : "7bit"
    );
    attachment->is_inline =
        g_ascii_strcasecmp(disposition, "inline") == 0;
    attachment->is_attachment =
        g_ascii_strcasecmp(disposition, "attachment") == 0;
    attachment->encoded_size = body_length;
    attachment->decoded_size = decoded_length;
    GDateTime *now = g_date_time_new_now_utc();
    attachment->extracted_at_utc = g_date_time_format_iso8601(now);
    g_date_time_unref(now);
    guint64 hashed_size = 0;
    if (!file_hash_compute_sha256(
            final_path,
            context->cancellable,
            &attachment->sha256,
            &hashed_size,
            error
        ))
    {
        g_remove(final_path);
        eml_attachment_free(attachment);
        g_free(canonical);
        g_free(sanitized);
        g_free(synthetic);
        goto fatal;
    }
    context->total_decoded_size += decoded_length;
    g_ptr_array_add(context->result->attachments, attachment);
    g_free(canonical);
    g_free(sanitized);
    g_free(synthetic);

cleanup:
    g_clear_pointer(&decoded, g_bytes_unref);
    g_hash_table_unref(disposition_parameters);
    g_free(disposition);
    g_free(filename);
    g_free(raw_filename);
    g_free(type_name);
    return TRUE;

fatal:
    g_clear_pointer(&decoded, g_bytes_unref);
    g_hash_table_unref(disposition_parameters);
    g_free(disposition);
    g_free(filename);
    g_free(raw_filename);
    g_free(type_name);
    return FALSE;
}

static gboolean eml_mime_parse_entity(
    EmlMimeContext *context,
    const char *data,
    gsize length,
    const char *part_index,
    guint depth,
    GError **error
);

static gboolean eml_mime_parse_multipart(
    EmlMimeContext *context,
    const char *body,
    gsize body_length,
    const char *boundary,
    const char *part_index,
    guint depth,
    GError **error
)
{
    char *delimiter = g_strdup_printf("--%s", boundary);
    gsize delimiter_length = strlen(delimiter);
    gsize cursor = 0;
    gsize part_start = 0;
    guint child_index = 0;
    gboolean opened = FALSE;
    gboolean closed = FALSE;

    while (cursor <= body_length)
    {
        gsize line_start = cursor;
        gsize line_length = 0;
        while (cursor < body_length && body[cursor] != '\n')
            cursor++;
        line_length = cursor - line_start;
        if (cursor < body_length)
            cursor++;
        if (line_length > 0 && body[line_start + line_length - 1] == '\r')
            line_length--;

        if (line_length >= delimiter_length &&
            memcmp(body + line_start, delimiter, delimiter_length) == 0 &&
            (line_length == delimiter_length ||
             (line_length == delimiter_length + 2 &&
              body[line_start + delimiter_length] == '-' &&
              body[line_start + delimiter_length + 1] == '-')))
        {
            if (opened && part_start < line_start)
            {
                gsize part_length = line_start - part_start;
                while (part_length > 0 &&
                       (body[part_start + part_length - 1] == '\r' ||
                        body[part_start + part_length - 1] == '\n'))
                    part_length--;
                child_index++;
                char *child_path = g_strdup_printf(
                    "%s.%u",
                    part_index,
                    child_index
                );
                gboolean success = eml_mime_parse_entity(
                    context,
                    body + part_start,
                    part_length,
                    child_path,
                    depth + 1,
                    error
                );
                g_free(child_path);
                if (!success)
                {
                    g_free(delimiter);
                    return FALSE;
                }
            }
            opened = TRUE;
            if (line_length == delimiter_length + 2)
            {
                closed = TRUE;
                break;
            }
            part_start = cursor;
        }
        if (cursor == body_length)
            break;
    }

    if (opened && !closed && part_start < body_length)
    {
        gsize part_length = body_length - part_start;
        while (part_length > 0 &&
               (body[part_start + part_length - 1] == '\r' ||
                body[part_start + part_length - 1] == '\n'))
            part_length--;
        child_index++;
        char *child_path = g_strdup_printf(
            "%s.%u",
            part_index,
            child_index
        );
        gboolean success = eml_mime_parse_entity(
            context,
            body + part_start,
            part_length,
            child_path,
            depth + 1,
            error
        );
        g_free(child_path);
        if (!success)
        {
            g_free(delimiter);
            return FALSE;
        }
    }

    if (!opened)
        eml_mime_warn(
            context,
            part_index,
            "le multipart déclaré ne contient aucune boundary."
        );
    else if (!closed)
        eml_mime_warn(
            context,
            part_index,
            "la boundary multipart n'est pas fermée."
        );
    g_free(delimiter);
    return TRUE;
}

static gboolean eml_mime_parse_entity(
    EmlMimeContext *context,
    const char *data,
    gsize length,
    const char *part_index,
    guint depth,
    GError **error
)
{
    gsize body_offset = 0;
    EmlMimeHeaders *headers = NULL;
    const char *raw_content_type = NULL;
    char *content_type = NULL;
    gboolean type_malformed = FALSE;
    GHashTable *type_parameters = NULL;
    gboolean success = TRUE;

    if (eml_mime_check_cancelled(context->cancellable, error))
        return FALSE;
    if (depth > EML_MIME_MAX_DEPTH)
    {
        eml_mime_warn(
            context,
            part_index,
            "la profondeur MIME maximale est dépassée."
        );
        return TRUE;
    }
    context->part_count++;
    if (context->part_count > EML_MIME_MAX_PARTS)
    {
        eml_mime_warn(
            context,
            part_index,
            "le nombre total maximal de parties est dépassé."
        );
        return TRUE;
    }

    GError *header_error = NULL;
    headers = eml_mime_parse_headers(
        data,
        length,
        &body_offset,
        &header_error
    );
    if (headers == NULL)
    {
        eml_mime_warn(context, part_index, header_error->message);
        g_clear_error(&header_error);
        return TRUE;
    }
    raw_content_type = eml_mime_headers_get(headers, "content-type");
    type_parameters = eml_mime_parse_parameters(
        raw_content_type != NULL ? raw_content_type : "text/plain",
        &content_type,
        &type_malformed
    );
    char *lower_content_type = g_ascii_strdown(content_type, -1);
    g_free(content_type);
    content_type = lower_content_type;
    if (type_malformed)
        eml_mime_warn(
            context,
            part_index,
            "Content-Type contient un paramètre malformé."
        );

    if (g_str_has_prefix(content_type, "multipart/"))
    {
        const char *boundary = g_hash_table_lookup(
            type_parameters,
            "boundary"
        );
        if (boundary == NULL || boundary[0] == '\0')
            eml_mime_warn(
                context,
                part_index,
                "le multipart ne déclare aucune boundary."
            );
        else
            success = eml_mime_parse_multipart(
                context,
                data + body_offset,
                length - body_offset,
                boundary,
                part_index,
                depth,
                error
            );
    }
    else if (g_str_equal(content_type, "message/rfc822"))
    {
        char *child_path = g_strdup_printf("%s.1", part_index);
        success = eml_mime_parse_entity(
            context,
            data + body_offset,
            length - body_offset,
            child_path,
            depth + 1,
            error
        );
        g_free(child_path);
    }
    else
        success = eml_mime_extract_leaf(
            context,
            headers,
            data + body_offset,
            length - body_offset,
            part_index,
            content_type,
            type_parameters,
            error
        );

    g_hash_table_unref(type_parameters);
    g_free(content_type);
    eml_mime_headers_free(headers);
    return success;
}

EmlMimeResult *eml_mime_extract_attachments_cancellable(
    const char *eml_path,
    const char *target_dir,
    GCancellable *cancellable,
    GError **error
)
{
    GMappedFile *mapped = NULL;
    EmlMimeContext context = { 0 };
    EmlMimeResult *result = NULL;
    gsize size = 0;

    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (eml_path == NULL || eml_path[0] == '\0' ||
        target_dir == NULL || target_dir[0] == '\0')
    {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_ARGUMENT,
            "Les chemins de l'EML et du dossier cible sont invalides."
        );
        return NULL;
    }
    if (eml_mime_check_cancelled(cancellable, error))
        return NULL;

    mapped = g_mapped_file_new(eml_path, FALSE, error);
    if (mapped == NULL)
        return NULL;
    size = g_mapped_file_get_length(mapped);
    if (size == 0 || size > EML_MIME_MAX_FILE_SIZE)
    {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_DATA,
            "Le fichier EML est vide ou dépasse la limite autorisée."
        );
        g_mapped_file_unref(mapped);
        return NULL;
    }
    if (g_mkdir_with_parents(target_dir, 0755) != 0)
    {
        g_set_error(
            error,
            G_FILE_ERROR,
            g_file_error_from_errno(errno),
            "Impossible de créer le dossier cible : %s",
            g_strerror(errno)
        );
        g_mapped_file_unref(mapped);
        return NULL;
    }

    result = g_new0(EmlMimeResult, 1);
    result->attachments = g_ptr_array_new_with_free_func(
        (GDestroyNotify) eml_attachment_free
    );
    result->warnings = g_ptr_array_new_with_free_func(g_free);
    context.target_dir = target_dir;
    context.target_canonical = g_canonicalize_filename(target_dir, NULL);
    context.cancellable = cancellable;
    context.result = result;

    if (!eml_mime_parse_entity(
            &context,
            g_mapped_file_get_contents(mapped),
            size,
            "1",
            1,
            error
        ))
    {
        g_free((gpointer) context.target_canonical);
        g_mapped_file_unref(mapped);
        eml_mime_result_free(result);
        return NULL;
    }
    g_free((gpointer) context.target_canonical);
    g_mapped_file_unref(mapped);
    return result;
}

EmlMimeResult *eml_mime_extract_attachments(
    const char *eml_path,
    const char *target_dir,
    GError **error
)
{
    return eml_mime_extract_attachments_cancellable(
        eml_path,
        target_dir,
        NULL,
        error
    );
}
