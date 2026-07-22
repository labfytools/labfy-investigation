/******************************************************************************
 * @file test_eml_analyzer.c
 * @brief Tests de l'analyse locale d'en-têtes EML synthétiques.
 ******************************************************************************/
#include "core/eml_analyzer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
/** @brief Vérifie le dépliage et les indicateurs extraits. */
static void test_eml_analyzer_headers(void)
{
    static const char content[] =
        "From: Example Sender <sender@example.test>\r\n"
        "Reply-To: replies@reply.test\r\n"
        "To: victim@example.net\r\n"
        "Subject: Synthetic fixture\r\n"
        "Message-ID: <id-123@example.test>\r\n"
        "Received: from mail.example.test (mail.example.test [192.0.2.10])\r\n"
        " by mx.example.net ([198.51.100.20]) with ESMTP; Wed, 22 Jul 2026 10:00:00 +0000\r\n"
        "Received: from localhost ([127.0.0.1]) by mail.example.test\r\n"
        "Authentication-Results: mx.example.net; spf=pass; dkim=pass; dmarc=pass\r\n"
        "\r\nBody must not be parsed: hidden@body.test\r\n";
    char *directory = NULL, *path = NULL;
    EmlAnalysis *analysis = NULL;
    const GPtrArray *received = NULL, *emails = NULL, *ips = NULL;
    const GPtrArray *sender_ips = NULL, *destination_ips = NULL;
    GError *error = NULL;
    directory = g_dir_make_tmp("labfy-eml-test-XXXXXX", &error);
    assert(directory != NULL && error == NULL);
    path = g_build_filename(directory, "synthetic.eml", NULL);
    assert(g_file_set_contents(path, content, -1, &error));
    analysis = eml_analyzer_analyze_file(path, &error);
    assert(analysis != NULL && error == NULL);
    assert(strcmp(eml_analysis_get_first_header(analysis, "from"),
        "Example Sender <sender@example.test>") == 0);
    received = eml_analysis_get_header_values(analysis, "Received");
    assert(received != NULL && received->len == 2);
    assert(strstr(g_ptr_array_index((GPtrArray *) received, 0), " by mx.example.net") != NULL);
    emails = eml_analysis_get_email_addresses(analysis);
    assert(emails != NULL && emails->len >= 3);
    for (guint i = 0; i < emails->len; i++)
        assert(strcmp(g_ptr_array_index((GPtrArray *) emails, i),
            "hidden@body.test") != 0);
    ips = eml_analysis_get_ip_addresses(analysis);
    assert(ips != NULL && ips->len == 3);
    sender_ips = eml_analysis_get_sender_ip_addresses(analysis);
    destination_ips = eml_analysis_get_destination_ip_addresses(analysis);
    assert(sender_ips != NULL && sender_ips->len == 2);
    assert(destination_ips != NULL && destination_ips->len == 1);
    assert(strcmp(g_ptr_array_index((GPtrArray *) sender_ips, 0),
        "192.0.2.10") == 0);
    assert(strcmp(g_ptr_array_index((GPtrArray *) destination_ips, 0),
        "198.51.100.20") == 0);
    eml_analysis_free(analysis);
    assert(g_remove(path) == 0); assert(g_rmdir(directory) == 0);
    g_free(path); g_free(directory);
}
int main(void)
{
    test_eml_analyzer_headers();
    puts("EmlAnalyzer : tous les tests sont valides."); return 0;
}
