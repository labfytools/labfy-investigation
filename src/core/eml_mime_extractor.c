/******************************************************************************
 * @file eml_mime_extractor.c
 * @brief Extraction MIME sécurisée et inventaire des pièces jointes d'un EML.
 ******************************************************************************/
#include "core/eml_mime_extractor.h"
#include "core/file_hash.h"
#include <gio/gio.h>
#include <glib.h>
#include <string.h>

#define EML_MIME_MAX_FILE_SIZE (50U * 1024U * 1024U)
#define EML_MIME_MAX_PARTS 100U
#define EML_MIME_MAX_DEPTH 10U

void eml_attachment_free(EmlAttachment *attachment)
{
    if (attachment == NULL)
        return;
    g_free(attachment->part_index);
    g_free(attachment->declared_filename);
    g_free(attachment->sanitized_filename);
    g_free(attachment->extracted_path);
    g_free(attachment->relative_path);
    g_free(attachment->content_type);
    g_free(attachment->detected_mime);
    g_free(attachment->content_id);
    g_free(attachment->transfer_encoding);
    g_free(attachment->sha256);
    g_free(attachment);
}

void eml_mime_result_free(EmlMimeResult *result)
{
    if (result == NULL)
        return;
    if (result->attachments != NULL)
        g_ptr_array_unref(result->attachments);
    if (result->warnings != NULL)
        g_ptr_array_unref(result->warnings);
    g_free(result);
}

char *eml_mime_sanitize_filename(const char *raw_filename)
{
    if (raw_filename == NULL || raw_filename[0] == '\0')
        return g_strdup("attachment.bin");

    char *clean = g_strdup(raw_filename);

    /* Décodage simple RFC 2047 si présent =?...?= */
    if (strstr(clean, "=?") != NULL)
    {
        /* Traitement basique ou suppression de préfixe */
        char *start = strstr(clean, "?B?");
        if (start == NULL) start = strstr(clean, "?b?");
        if (start != NULL)
        {
            char *end = strstr(start + 3, "?=");
            if (end != NULL)
            {
                *end = '\0';
                gsize out_len = 0;
                guchar *decoded = g_base64_decode(start + 3, &out_len);
                if (decoded != NULL && out_len > 0)
                {
                    char *valid = g_utf8_make_valid((const char *) decoded, (gssize) out_len);
                    g_free(clean);
                    clean = valid;
                    g_free(decoded);
                }
            }
        }
    }

    /* Remplacement des caractères dangereux ou des séparateurs de chemin */
    gsize len = strlen(clean);
    GString *sanitized = g_string_new_len(NULL, (gssize) len);

    for (gsize i = 0; i < len; i++)
    {
        char c = clean[i];
        if (c == '/' || c == '\\' || c == ':' || c == '\0' || c == '\r' || c == '\n' || c == '\t')
        {
            g_string_append_c(sanitized, '_');
        }
        else
        {
            g_string_append_c(sanitized, c);
        }
    }
    g_free(clean);

    /* Suppression des séquences '..' */
    char *res = g_strdup(sanitized->str);
    g_string_free(sanitized, TRUE);

    while (strstr(res, "..") != NULL)
    {
        char *pos = strstr(res, "..");
        pos[0] = '_';
        pos[1] = '_';
    }

    g_strstrip(res);
    if (res[0] == '\0' || strcmp(res, ".") == 0 || strcmp(res, "..") == 0)
    {
        g_free(res);
        return g_strdup("attachment.bin");
    }

    return res;
}

static GBytes *decode_transfer_encoding(const char *encoding, const char *raw_data, gsize raw_len)
{
    if (encoding != NULL && g_ascii_strcasecmp(encoding, "base64") == 0)
    {
        gsize out_len = 0;
        guchar *decoded = g_base64_decode(raw_data, &out_len);
        if (decoded != NULL)
        {
            return g_bytes_new_take(decoded, out_len);
        }
    }

    if (encoding != NULL &&
        g_ascii_strcasecmp(encoding, "quoted-printable") == 0)
    {
        GByteArray *decoded = g_byte_array_new();
        for (gsize index = 0; index < raw_len; index++)
        {
            if (raw_data[index] == '=' && index + 2 < raw_len &&
                raw_data[index + 1] == '\r' && raw_data[index + 2] == '\n')
            {
                index += 2;
                continue;
            }
            if (raw_data[index] == '=' && index + 2 < raw_len &&
                g_ascii_isxdigit(raw_data[index + 1]) &&
                g_ascii_isxdigit(raw_data[index + 2]))
            {
                char hex[3] = { raw_data[index + 1], raw_data[index + 2], 0 };
                guint8 value = (guint8) g_ascii_strtoll(hex, NULL, 16);
                g_byte_array_append(decoded, &value, 1);
                index += 2;
                continue;
            }
            g_byte_array_append(decoded, (const guint8 *) &raw_data[index], 1);
        }
        return g_byte_array_free_to_bytes(decoded);
    }

    /* Traitement par défaut ou quoted-printable simple */
    return g_bytes_new(raw_data, raw_len);
}

EmlMimeResult *eml_mime_extract_attachments(const char *eml_path,
                                             const char *target_dir,
                                             GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (eml_path == NULL || eml_path[0] == '\0' || target_dir == NULL || target_dir[0] == '\0')
    {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                            "Les chemins de l'EML et du dossier cible doivent être valides.");
        return NULL;
    }

    GMappedFile *mapped = g_mapped_file_new(eml_path, FALSE, error);
    if (mapped == NULL)
        return NULL;

    gsize size = g_mapped_file_get_length(mapped);
    const char *data = g_mapped_file_get_contents(mapped);

    if (size == 0 || size > EML_MIME_MAX_FILE_SIZE)
    {
        g_mapped_file_unref(mapped);
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                            "Fichier EML vide ou trop volumineux.");
        return NULL;
    }

    if (g_mkdir_with_parents(target_dir, 0755) != 0)
    {
        g_mapped_file_unref(mapped);
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_ACCES,
                            "Impossible de créer le dossier de destination des pièces jointes.");
        return NULL;
    }

    EmlMimeResult *result = g_new0(EmlMimeResult, 1);
    result->attachments = g_ptr_array_new_with_free_func((GDestroyNotify) eml_attachment_free);
    result->warnings = g_ptr_array_new_with_free_func(g_free);

    /* Détection de boundary MIME si multipart */
    const char *boundary_key = "boundary=";
    const char *b_pos = strstr(data, boundary_key);
    char *boundary = NULL;

    if (b_pos != NULL)
    {
        const char *b_start = b_pos + strlen(boundary_key);
        if (*b_start == '"')
        {
            b_start++;
            const char *b_end = strchr(b_start, '"');
            if (b_end != NULL)
                boundary = g_strndup(b_start, (gsize)(b_end - b_start));
        }
        else
        {
            const char *b_end = b_start;
            while (*b_end && *b_end != '\r' && *b_end != '\n' && *b_end != ';')
                b_end++;
            boundary = g_strndup(b_start, (gsize)(b_end - b_start));
        }
    }

    if (boundary != NULL)
    {
        char *delimiter = g_strdup_printf("--%s", boundary);
        char **parts = g_strsplit(data, delimiter, EML_MIME_MAX_PARTS);
        g_free(delimiter);
        g_free(boundary);

        for (guint i = 1; parts[i] != NULL && parts[i][0] != '\0'; i++)
        {
            if (strncmp(parts[i], "--", 2) == 0)
                break; /* Fin du multipart */

            const char *part_content = parts[i];
            const char *hdr_end = strstr(part_content, "\r\n\r\n");
            if (hdr_end == NULL) hdr_end = strstr(part_content, "\n\n");
            if (hdr_end == NULL) continue;

            gsize hdr_len = (gsize)(hdr_end - part_content);
            char *headers = g_strndup(part_content, hdr_len);
            const char *body = hdr_end + (strstr(hdr_end, "\r\n\r\n") == hdr_end ? 4 : 2);

            /* Extraction du nom de fichier et content-type */
            char *filename = NULL;
            const char *fn_pos = strstr(headers, "filename=");
            if (fn_pos == NULL) fn_pos = strstr(headers, "name=");
            if (fn_pos != NULL)
            {
                const char *fn_start = fn_pos + (strstr(fn_pos, "filename=") == fn_pos ? 9 : 5);
                if (*fn_start == '"')
                {
                    fn_start++;
                    const char *fn_end = strchr(fn_start, '"');
                    if (fn_end != NULL) filename = g_strndup(fn_start, (gsize)(fn_end - fn_start));
                }
                else
                {
                    const char *fn_end = fn_start;
                    while (*fn_end && *fn_end != '\r' && *fn_end != '\n' && *fn_end != ';') fn_end++;
                    filename = g_strndup(fn_start, (gsize)(fn_end - fn_start));
                }
            }

            gboolean is_attachment = (strstr(headers, "attachment") != NULL) || (filename != NULL);
            if (is_attachment)
            {
                char *sanitized = eml_mime_sanitize_filename(filename);
                char *dest_path = g_build_filename(target_dir, sanitized, NULL);

                /* Gestion des collisions */
                guint counter = 1;
                while (g_file_test(dest_path, G_FILE_TEST_EXISTS))
                {
                    g_free(dest_path);
                    char *new_name = g_strdup_printf("%u_%s", counter++, sanitized);
                    dest_path = g_build_filename(target_dir, new_name, NULL);
                    g_free(new_name);
                }

                /* Extraction du Content-Transfer-Encoding */
                char *encoding = NULL;
                const char *enc_pos = strstr(headers, "Content-Transfer-Encoding:");
                if (enc_pos != NULL)
                {
                    const char *enc_val = enc_pos + 26;
                    const char *enc_end = strchr(enc_val, '\n');
                    if (enc_end != NULL) encoding = g_strndup(enc_val, (gsize)(enc_end - enc_val));
                    if (encoding != NULL) g_strstrip(encoding);
                }

                /* Extraction du Content-Type */
                char *content_type = NULL;
                const char *ct_pos = strstr(headers, "Content-Type:");
                if (ct_pos != NULL)
                {
                    const char *ct_val = ct_pos + 13;
                    const char *ct_end = strchr(ct_val, ';');
                    if (ct_end == NULL) ct_end = strchr(ct_val, '\n');
                    if (ct_end != NULL) content_type = g_strndup(ct_val, (gsize)(ct_end - ct_val));
                    if (content_type != NULL) g_strstrip(content_type);
                }

                /* Extraction du Content-ID */
                char *content_id = NULL;
                const char *cid_pos = strstr(headers, "Content-ID:");
                if (cid_pos != NULL)
                {
                    const char *cid_val = cid_pos + 11;
                    const char *cid_end = strchr(cid_val, '\n');
                    if (cid_end != NULL) content_id = g_strndup(cid_val, (gsize)(cid_end - cid_val));
                    if (content_id != NULL) g_strstrip(content_id);
                }

                gsize body_len = strlen(body);
                GBytes *decoded_bytes = decode_transfer_encoding(encoding, body, body_len);
                gsize decoded_len = 0;
                gconstpointer decoded_data = g_bytes_get_data(decoded_bytes, &decoded_len);

                GError *write_error = NULL;
                if (g_file_set_contents(dest_path, decoded_data, (gssize) decoded_len, &write_error))
                {
                    char *sha256 = NULL;
                    guint64 file_size = 0;
                    file_hash_compute_sha256(dest_path, NULL, &sha256,
                        &file_size, NULL);

                    EmlAttachment *att = g_new0(EmlAttachment, 1);
                    att->part_index = g_strdup_printf("1.%u", i);
                    att->declared_filename = filename != NULL ? g_strdup(filename) : g_strdup("attachment.bin");
                    att->sanitized_filename = g_strdup(sanitized);
                    att->extracted_path = dest_path; dest_path = NULL;
                    att->content_type = content_type != NULL ? content_type : g_strdup("application/octet-stream"); content_type = NULL;
                    att->content_id = content_id; content_id = NULL;
                    att->transfer_encoding = encoding != NULL ? encoding : g_strdup("7bit"); encoding = NULL;
                    att->is_inline = (strstr(headers, "inline") != NULL);
                    att->encoded_size = body_len;
                    att->decoded_size = decoded_len;
                    att->sha256 = sha256;
                    {
                        char *target_name = g_path_get_basename(target_dir);
                        att->relative_path = g_build_filename(
                            "02_Preuves_Traitees", "eml_attachments",
                            target_name, sanitized, NULL);
                        g_free(target_name);
                    }
                    if (content_type != NULL &&
                        g_content_type_is_a(content_type, "text/plain"))
                        att->detected_mime = g_strdup("text/plain");
                    else
                        att->detected_mime = g_content_type_guess(
                            dest_path != NULL ? dest_path : att->extracted_path,
                            decoded_data, decoded_len, NULL);
                    if (att->detected_mime == NULL)
                        att->detected_mime = g_strdup("application/octet-stream");
                    {
                        const char *dot = strrchr(att->sanitized_filename, '.');
                        if (dot != NULL && g_ascii_strcasecmp(dot + 1, "txt") == 0 &&
                            g_strcmp0(att->detected_mime, "text/plain") != 0)
                            att->has_inconsistency = TRUE;
                    }

                    g_ptr_array_add(result->attachments, att);
                }
                else
                {
                    char *warn = g_strdup_printf("Impossible d'écrire la pièce jointe %s : %s",
                                                 sanitized, write_error->message);
                    g_ptr_array_add(result->warnings, warn);
                    g_error_free(write_error);
                    g_free(dest_path);
                }

                g_bytes_unref(decoded_bytes);
                g_free(content_type);
                g_free(content_id);
                g_free(encoding);
                g_free(sanitized);
            }

            g_free(filename);
            g_free(headers);
        }

        g_strfreev(parts);
    }

    g_mapped_file_unref(mapped);
    return result;
}
