/******************************************************************************
 * @file test_eml_mime_extractor.c
 * @brief Tests synthétiques de l'extracteur MIME récursif.
 ******************************************************************************/
#include "core/eml_mime_extractor.h"

#include <glib.h>
#include <glib/gstdio.h>

typedef struct
{
    char *directory;
    char *eml_path;
    char *output_directory;
} MimeFixture;

static MimeFixture *mime_fixture_new(const char *content)
{
    GError *error = NULL;
    MimeFixture *fixture = g_new0(MimeFixture, 1);
    fixture->directory = g_dir_make_tmp("labfy-mime-XXXXXX", &error);
    g_assert_no_error(error);
    fixture->eml_path = g_build_filename(
        fixture->directory,
        "synthetic.eml",
        NULL
    );
    fixture->output_directory = g_build_filename(
        fixture->directory,
        "derived",
        NULL
    );
    g_assert_true(g_file_set_contents(
        fixture->eml_path,
        content,
        -1,
        &error
    ));
    g_assert_no_error(error);
    return fixture;
}

static void mime_fixture_free(MimeFixture *fixture)
{
    GDir *directory = g_dir_open(fixture->output_directory, 0, NULL);
    if (directory != NULL)
    {
        const char *name = NULL;
        while ((name = g_dir_read_name(directory)) != NULL)
        {
            char *path = g_build_filename(
                fixture->output_directory,
                name,
                NULL
            );
            g_assert_cmpint(g_remove(path), ==, 0);
            g_free(path);
        }
        g_dir_close(directory);
        g_assert_cmpint(g_rmdir(fixture->output_directory), ==, 0);
    }
    g_assert_cmpint(g_remove(fixture->eml_path), ==, 0);
    g_assert_cmpint(g_rmdir(fixture->directory), ==, 0);
    g_free(fixture->output_directory);
    g_free(fixture->eml_path);
    g_free(fixture->directory);
    g_free(fixture);
}

static char *attachment_contents(EmlAttachment *attachment)
{
    char *contents = NULL;
    GError *error = NULL;
    g_assert_true(g_file_get_contents(
        attachment->extracted_path,
        &contents,
        NULL,
        &error
    ));
    g_assert_no_error(error);
    return contents;
}

static void test_nested_order_and_encodings(void)
{
    static const char eml[] =
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=outer\r\n\r\n"
        "--outer\r\nContent-Type: text/plain\r\n\r\nbody\r\n"
        "--outer\r\n"
        "Content-Type: multipart/related; boundary=inner\r\n\r\n"
        "--inner\r\nContent-Type: image/png\r\n"
        "Content-Disposition: inline\r\n"
        "Content-ID: <synthetic-image@test.invalid>\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n"
        "UE5H\r\n"
        "--inner\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment;\r\n"
        " filename*0*=UTF-8''rapport%20;\r\n"
        " filename*1*=synth%C3%A9tique.txt\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
        "ligne=20une=\r\nligne=20deux\r\n"
        "--inner--\r\n"
        "--outer\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment;\r\n"
        " filename=\"=?UTF-8?Q?troisi=C3=A8me.txt?=\"\r\n\r\n"
        "third\r\n--outer--\r\n";
    MimeFixture *fixture = mime_fixture_new(eml);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpuint(result->attachments->len, ==, 3);

    EmlAttachment *first = g_ptr_array_index(result->attachments, 0);
    EmlAttachment *second = g_ptr_array_index(result->attachments, 1);
    EmlAttachment *third = g_ptr_array_index(result->attachments, 2);
    g_assert_cmpstr(first->part_index, ==, "1.2.1");
    g_assert_true(first->is_inline);
    g_assert_false(first->is_attachment);
    g_assert_cmpstr(first->normalized_content_id, ==,
        "synthetic-image@test.invalid");
    g_assert_cmpstr(second->part_index, ==, "1.2.2");
    g_assert_cmpstr(second->sanitized_filename, ==,
        "rapport synthétique.txt");
    g_assert_cmpstr(third->part_index, ==, "1.3");
    g_assert_cmpstr(third->sanitized_filename, ==, "troisième.txt");

    char *first_content = attachment_contents(first);
    char *second_content = attachment_contents(second);
    char *third_content = attachment_contents(third);
    g_assert_cmpstr(first_content, ==, "PNG");
    g_assert_cmpstr(second_content, ==, "ligne uneligne deux");
    g_assert_cmpstr(third_content, ==, "third");
    g_assert_nonnull(first->sha256);
    g_assert_nonnull(first->detected_mime);
    g_free(first_content);
    g_free(second_content);
    g_free(third_content);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_three_levels_and_message(void)
{
    static const char eml[] =
        "Content-Type: multipart/mixed; boundary=a\r\n\r\n"
        "--a\r\nContent-Type: multipart/alternative; boundary=b\r\n\r\n"
        "--b\r\nContent-Type: multipart/related; boundary=c\r\n\r\n"
        "--c\r\nContent-Type: text/plain; name=four.txt\r\n"
        "Content-Disposition: attachment\r\n\r\nfour\r\n--c--\r\n"
        "--b--\r\n--a\r\nContent-Type: message/rfc822\r\n\r\n"
        "Content-Type: text/plain; name=inside.txt\r\n"
        "Content-Disposition: attachment\r\n\r\ninside\r\n"
        "--a--\r\n";
    MimeFixture *fixture = mime_fixture_new(eml);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 2);
    EmlAttachment *first = g_ptr_array_index(result->attachments, 0);
    EmlAttachment *second = g_ptr_array_index(result->attachments, 1);
    g_assert_cmpstr(first->part_index, ==, "1.1.1.1");
    g_assert_cmpstr(second->part_index, ==, "1.2.1");
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_rfc2047_and_rfc2231_priorities(void)
{
    static const char eml[] =
        "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
        "--x\r\nContent-Type: text/plain; name=fallback.txt\r\n"
        "Content-Disposition: attachment; filename=plain.txt;\r\n"
        " filename*=ISO-8859-1''caf%E9.txt\r\n\r\none\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment;\r\n"
        " filename=\"ASCII =?UTF-8?B?w6l0dWRl?=.txt\"\r\n\r\ntwo\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment;\r\n"
        " filename*0=continued-; filename*1=name.txt\r\n\r\nthree\r\n"
        "--x\r\nContent-Type: text/plain;\r\n"
        " name*=UTF-8''type%20fallback.txt\r\n"
        "Content-Disposition: inline\r\n\r\nfour\r\n"
        "--x--\r\n";
    MimeFixture *fixture = mime_fixture_new(eml);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 4);
    g_assert_cmpstr(
        ((EmlAttachment *) g_ptr_array_index(
            result->attachments, 0))->sanitized_filename,
        ==,
        "café.txt"
    );
    g_assert_cmpstr(
        ((EmlAttachment *) g_ptr_array_index(
            result->attachments, 1))->sanitized_filename,
        ==,
        "ASCII étude.txt"
    );
    g_assert_cmpstr(
        ((EmlAttachment *) g_ptr_array_index(
            result->attachments, 2))->sanitized_filename,
        ==,
        "continued-name.txt"
    );
    g_assert_cmpstr(
        ((EmlAttachment *) g_ptr_array_index(
            result->attachments, 3))->sanitized_filename,
        ==,
        "type fallback.txt"
    );
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_invalid_encodings_are_warnings(void)
{
    static const char eml[] =
        "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
        "--x\r\nContent-Type: text/plain; name=a.txt\r\n"
        "Content-Disposition: attachment\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nA!AA\r\n"
        "--x\r\nContent-Type: text/plain; name=b.txt\r\n"
        "Content-Disposition: attachment\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n\r\nbad=QZ\r\n"
        "--x\r\nContent-Type: text/plain; name=c.txt\r\n"
        "Content-Disposition: attachment\r\n"
        "Content-Transfer-Encoding: synthetic\r\n\r\nbad\r\n"
        "--x--\r\n";
    MimeFixture *fixture = mime_fixture_new(eml);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 0);
    g_assert_cmpuint(result->warnings->len, ==, 3);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_paths_and_collisions(void)
{
    static const char eml[] =
        "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment; filename=\"../same.txt\"\r\n\r\n1\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment; filename=\"C:\\\\same.txt\"\r\n\r\n2\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment; filename=\"/same.txt\"\r\n\r\n3\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment; filename=\"../same.txt\"\r\n\r\n4\r\n"
        "--x--\r\n";
    MimeFixture *fixture = mime_fixture_new(eml);
    GError *error = NULL;
    g_assert_cmpint(g_mkdir_with_parents(
        fixture->output_directory, 0755), ==, 0);
    char *existing = g_build_filename(
        fixture->output_directory,
        "___same.txt",
        NULL
    );
    g_assert_true(g_file_set_contents(existing, "existing", -1, &error));
    g_assert_no_error(error);

    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 4);
    for (guint index = 0; index < result->attachments->len; index++)
    {
        EmlAttachment *attachment = g_ptr_array_index(
            result->attachments,
            index
        );
        g_assert_null(strchr(attachment->sanitized_filename, '/'));
        g_assert_null(strchr(attachment->sanitized_filename, '\\'));
    }
    char *existing_content = NULL;
    g_assert_true(g_file_get_contents(
        existing, &existing_content, NULL, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(existing_content, ==, "existing");
    g_free(existing_content);
    g_free(existing);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_malformed_and_incomplete_rfc2231(void)
{
    static const char eml[] =
        "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment; filename*0*=UTF-8''bad;\r\n"
        " filename*2*=gap.txt; filename=fallback.txt\r\n\r\nok\r\n"
        "--x\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment; filename*0*=UTF-8''one;\r\n"
        " filename*0*=duplicate; filename=duplicate-fallback.txt\r\n\r\ntwo\r\n";
    MimeFixture *fixture = mime_fixture_new(eml);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 2);
    EmlAttachment *attachment = g_ptr_array_index(result->attachments, 0);
    g_assert_cmpstr(attachment->sanitized_filename, ==, "fallback.txt");
    attachment = g_ptr_array_index(result->attachments, 1);
    g_assert_cmpstr(
        attachment->sanitized_filename,
        ==,
        "duplicate-fallback.txt"
    );
    g_assert_cmpuint(result->warnings->len, >=, 1);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_missing_boundary(void)
{
    static const char eml[] =
        "Content-Type: multipart/mixed\r\n\r\nnot structured";
    MimeFixture *fixture = mime_fixture_new(eml);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 0);
    g_assert_cmpuint(result->warnings->len, ==, 1);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_cancelled_before_extraction(void)
{
    static const char eml[] =
        "Content-Type: text/plain; name=a.txt\r\n"
        "Content-Disposition: attachment\r\n\r\ncontent";
    MimeFixture *fixture = mime_fixture_new(eml);
    GCancellable *cancellable = g_cancellable_new();
    GError *error = NULL;
    g_cancellable_cancel(cancellable);
    EmlMimeResult *result = eml_mime_extract_attachments_cancellable(
        fixture->eml_path,
        fixture->output_directory,
        cancellable,
        &error
    );
    g_assert_null(result);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    g_object_unref(cancellable);
    mime_fixture_free(fixture);
}

static void test_source_unchanged(void)
{
    static const char eml[] =
        "Content-Type: text/plain; name=a.txt\r\n"
        "Content-Disposition: attachment\r\n\r\nimmutable";
    MimeFixture *fixture = mime_fixture_new(eml);
    char *before = NULL;
    char *after = NULL;
    GError *error = NULL;
    g_assert_true(g_file_get_contents(
        fixture->eml_path, &before, NULL, &error));
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_true(g_file_get_contents(
        fixture->eml_path, &after, NULL, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(before, ==, after);
    g_free(before);
    g_free(after);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

static void test_filename_sanitizer_limits(void)
{
    char *empty = eml_mime_sanitize_filename("   ");
    char *unix_path = eml_mime_sanitize_filename("../../absolute/test");
    char *windows_path = eml_mime_sanitize_filename("C:\\temp\\test");
    char *long_name = g_strnfill(
        EML_MIME_MAX_FILENAME_LENGTH + 100,
        'a'
    );
    char *shortened = eml_mime_sanitize_filename(long_name);

    g_assert_cmpstr(empty, ==, "attachment.bin");
    g_assert_null(strchr(unix_path, '/'));
    g_assert_null(strchr(windows_path, '\\'));
    g_assert_cmpuint(
        strlen(shortened),
        <=,
        EML_MIME_MAX_FILENAME_LENGTH
    );
    g_free(empty);
    g_free(unix_path);
    g_free(windows_path);
    g_free(long_name);
    g_free(shortened);
}

static void test_part_count_limit(void)
{
    GString *eml = g_string_new(
        "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
    );
    for (guint index = 0; index < EML_MIME_MAX_PARTS + 4; index++)
        g_string_append_printf(
            eml,
            "--x\r\nContent-Type: text/plain; name=p%u.txt\r\n"
            "Content-Disposition: attachment\r\n\r\n%u\r\n",
            index,
            index
        );
    g_string_append(eml, "--x--\r\n");
    MimeFixture *fixture = mime_fixture_new(eml->str);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(
        result->attachments->len,
        ==,
        EML_MIME_MAX_PARTS - 1
    );
    g_assert_cmpuint(result->warnings->len, >, 0);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
    g_string_free(eml, TRUE);
}

static void test_depth_limit(void)
{
    GString *eml = g_string_new(NULL);
    for (guint depth = 1; depth <= EML_MIME_MAX_DEPTH + 1; depth++)
        g_string_append_printf(
            eml,
            "Content-Type: multipart/mixed; boundary=b%u\r\n\r\n--b%u\r\n",
            depth,
            depth
        );
    g_string_append(
        eml,
        "Content-Type: text/plain; name=too-deep.txt\r\n"
        "Content-Disposition: attachment\r\n\r\ndeep\r\n"
    );
    for (gint depth = (gint) EML_MIME_MAX_DEPTH + 1; depth >= 1; depth--)
        g_string_append_printf(eml, "--b%d--\r\n", depth);

    MimeFixture *fixture = mime_fixture_new(eml->str);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 0);
    g_assert_cmpuint(result->warnings->len, >, 0);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
    g_string_free(eml, TRUE);
}

static void test_part_size_and_no_temporary_file(void)
{
    GString *eml = g_string_new(
        "Content-Type: text/plain; name=large.txt\r\n"
        "Content-Disposition: attachment\r\n\r\n"
    );
    char *large_content = g_strnfill(
        EML_MIME_MAX_PART_DECODED_SIZE + 1,
        'x'
    );
    g_string_append_len(
        eml,
        large_content,
        EML_MIME_MAX_PART_DECODED_SIZE + 1
    );
    g_free(large_content);
    MimeFixture *fixture = mime_fixture_new(eml->str);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 0);
    g_assert_cmpuint(result->warnings->len, ==, 1);
    GDir *directory = g_dir_open(fixture->output_directory, 0, &error);
    g_assert_no_error(error);
    g_assert_null(g_dir_read_name(directory));
    g_dir_close(directory);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
    g_string_free(eml, TRUE);
}

static void test_empty_base64_and_malformed_header(void)
{
    static const char valid[] =
        "Content-Type: application/octet-stream; name=empty.bin\r\n"
        "Content-Disposition: attachment\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n";
    MimeFixture *fixture = mime_fixture_new(valid);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 1);
    EmlAttachment *attachment = g_ptr_array_index(result->attachments, 0);
    g_assert_cmpuint(attachment->decoded_size, ==, 0);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);

    fixture = mime_fixture_new("Malformed header\r\n\r\nbody");
    result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->warnings->len, ==, 1);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
}

typedef struct
{
    GCancellable *cancellable;
    const char *output_directory;
} CancellationData;

static gpointer cancel_when_extraction_starts(gpointer user_data)
{
    CancellationData *data = user_data;
    while (!g_file_test(data->output_directory, G_FILE_TEST_IS_DIR))
        g_thread_yield();
    g_cancellable_cancel(data->cancellable);
    return NULL;
}

static void test_cancelled_during_extraction(void)
{
    GString *eml = g_string_new(
        "Content-Type: text/plain; name=a.txt\r\n"
        "Content-Disposition: attachment\r\n\r\n"
    );
    char *large_content = g_strnfill(
        EML_MIME_MAX_PART_DECODED_SIZE,
        'c'
    );
    g_string_append_len(
        eml,
        large_content,
        EML_MIME_MAX_PART_DECODED_SIZE
    );
    g_free(large_content);
    MimeFixture *fixture = mime_fixture_new(eml->str);
    GCancellable *cancellable = g_cancellable_new();
    CancellationData data = {
        .cancellable = cancellable,
        .output_directory = fixture->output_directory
    };
    GThread *thread = g_thread_new(
        "mime-cancel",
        cancel_when_extraction_starts,
        &data
    );
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments_cancellable(
        fixture->eml_path,
        fixture->output_directory,
        cancellable,
        &error
    );
    g_thread_join(thread);
    g_assert_null(result);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    GDir *directory = g_dir_open(fixture->output_directory, 0, &error);
    g_assert_no_error(error);
    g_assert_null(g_dir_read_name(directory));
    g_dir_close(directory);
    g_object_unref(cancellable);
    mime_fixture_free(fixture);
    g_string_free(eml, TRUE);
}

static void test_total_decoded_limit(void)
{
    const gsize part_length = 7U * 1024U * 1024U;
    char *part_content = g_strnfill(part_length, 'z');
    GString *eml = g_string_new(
        "Content-Type: multipart/mixed; boundary=total\r\n\r\n"
    );
    for (guint index = 0; index < 5; index++)
    {
        g_string_append_printf(
            eml,
            "--total\r\nContent-Type: application/octet-stream; "
            "name=large-%u.bin\r\n"
            "Content-Disposition: attachment\r\n\r\n",
            index
        );
        g_string_append_len(eml, part_content, (gssize) part_length);
        g_string_append(eml, "\r\n");
    }
    g_string_append(eml, "--total--\r\n");
    g_free(part_content);

    MimeFixture *fixture = mime_fixture_new(eml->str);
    GError *error = NULL;
    EmlMimeResult *result = eml_mime_extract_attachments(
        fixture->eml_path,
        fixture->output_directory,
        &error
    );
    g_assert_no_error(error);
    g_assert_cmpuint(result->attachments->len, ==, 4);
    g_assert_cmpuint(result->warnings->len, ==, 1);
    eml_mime_result_free(result);
    mime_fixture_free(fixture);
    g_string_free(eml, TRUE);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(
        "/eml-mime/nested-order-encodings",
        test_nested_order_and_encodings
    );
    g_test_add_func(
        "/eml-mime/three-levels-message",
        test_three_levels_and_message
    );
    g_test_add_func(
        "/eml-mime/rfc2047-rfc2231-priorities",
        test_rfc2047_and_rfc2231_priorities
    );
    g_test_add_func(
        "/eml-mime/invalid-encodings",
        test_invalid_encodings_are_warnings
    );
    g_test_add_func(
        "/eml-mime/paths-collisions",
        test_paths_and_collisions
    );
    g_test_add_func(
        "/eml-mime/malformed-rfc2231",
        test_malformed_and_incomplete_rfc2231
    );
    g_test_add_func(
        "/eml-mime/missing-boundary",
        test_missing_boundary
    );
    g_test_add_func(
        "/eml-mime/cancelled-before",
        test_cancelled_before_extraction
    );
    g_test_add_func(
        "/eml-mime/source-unchanged",
        test_source_unchanged
    );
    g_test_add_func(
        "/eml-mime/filename-sanitizer-limits",
        test_filename_sanitizer_limits
    );
    g_test_add_func(
        "/eml-mime/part-count-limit",
        test_part_count_limit
    );
    g_test_add_func(
        "/eml-mime/depth-limit",
        test_depth_limit
    );
    g_test_add_func(
        "/eml-mime/part-size-no-temporary",
        test_part_size_and_no_temporary_file
    );
    g_test_add_func(
        "/eml-mime/empty-base64-malformed-header",
        test_empty_base64_and_malformed_header
    );
    g_test_add_func(
        "/eml-mime/cancelled-during",
        test_cancelled_during_extraction
    );
    g_test_add_func(
        "/eml-mime/total-decoded-limit",
        test_total_decoded_limit
    );
    return g_test_run();
}
