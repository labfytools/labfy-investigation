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
        "Date: Wed, 22 Jul 2026 12:00:00 +0200\r\n"
        "Message-ID: <id-123@example.test>\r\n"
        "MIME-Version: 1.0\r\n"
        "Received: from mail.example.test (mail.example.test [192.0.2.10])\r\n"
        " by mx.example.net ([198.51.100.20]) with ESMTP; Wed, 22 Jul 2026 10:00:00 +0000\r\n"
        "Received: from localhost ([127.0.0.1]) by mail.example.test\r\n"
        "Authentication-Results: mx.example.net; spf=pass; dkim=pass; dmarc=pass\r\n"
        "\r\nBody must not be parsed: hidden@body.test\r\n";
    char *directory = NULL, *path = NULL;
    EmlAnalysis *analysis = NULL;
    const GPtrArray *received = NULL, *emails = NULL, *ips = NULL;
    const GPtrArray *sender_ips = NULL, *destination_ips = NULL;
    const GPtrArray *domains = NULL, *observations = NULL;
    GError *error = NULL;
    directory = g_dir_make_tmp("labfy-eml-test-XXXXXX", &error);
    assert(directory != NULL && error == NULL);
    path = g_build_filename(directory, "synthetic.eml", NULL);
    assert(g_file_set_contents(path, content, -1, &error));
    analysis = eml_analyzer_analyze_file(path, &error);
    assert(analysis != NULL && error == NULL);
    assert(strcmp(eml_analysis_get_first_header(analysis, "from"),
        "Example Sender <sender@example.test>") == 0);
    assert(strcmp(eml_analysis_get_date_utc(analysis),
        "2026-07-22T10:00:00Z") == 0);
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
    domains = eml_analysis_get_domains(analysis);
    for (guint i = 0; i < domains->len; i++)
    {
        const char *domain = g_ptr_array_index((GPtrArray *) domains, i);
        assert(strcmp(domain, "192.0.2.10") != 0);
        assert(strcmp(domain, "198.51.100.20") != 0);
        assert(strcmp(domain, "1.0") != 0);
    }
    observations = eml_analysis_get_observations(analysis);
    assert(observations != NULL && observations->len > 0);
    gboolean found_from = FALSE, found_received_ip = FALSE;
    for (guint i = 0; i < observations->len; i++)
    {
        const EmlObservation *observation = g_ptr_array_index(
            (GPtrArray *) observations, i);
        if (strcmp(observation->value_normalized, "sender@example.test") == 0 &&
            strcmp(observation->role, "from") == 0 &&
            strcmp(observation->source_header, "from") == 0)
            found_from = TRUE;
        if (strcmp(observation->value_normalized, "192.0.2.10") == 0 &&
            strcmp(observation->role, "smtp_relay") == 0 &&
            strcmp(observation->source_header, "received") == 0)
            found_received_ip = TRUE;
    }
    assert(found_from && found_received_ip);
    eml_analysis_free(analysis);
    assert(g_remove(path) == 0); assert(g_rmdir(directory) == 0);
    g_free(path); g_free(directory);
}
static void test_eml_analyzer_manual_fixture_regression(void)
{
    GError *error = NULL;
    EmlAnalysis *analysis = eml_analyzer_analyze_file(
        "tests/fixtures/eml/manual_smoke_test.eml", &error);
    assert(analysis != NULL && error == NULL);
    const GPtrArray *domains = eml_analysis_get_domains(analysis);
    for (guint index = 0; index < domains->len; index++)
    {
        const char *domain = g_ptr_array_index((GPtrArray *) domains, index);
        assert(strcmp(domain, "192.0.2.10") != 0);
        assert(strcmp(domain, "198.51.100.20") != 0);
        assert(strcmp(domain, "1.0") != 0);
    }
    const GPtrArray *observations = eml_analysis_get_observations(analysis);
    assert(observations != NULL && observations->len >= 10);
    eml_analysis_free(analysis);
}
int main(void)
{
    test_eml_analyzer_headers();
    test_eml_analyzer_manual_fixture_regression();
    puts("EmlAnalyzer : tous les tests sont valides."); return 0;
}
