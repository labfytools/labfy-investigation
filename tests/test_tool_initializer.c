/******************************************************************************
 * @file test_tool_initializer.c
 * @brief Tests du composant ToolInitializer.
 ******************************************************************************/

#include "core/tool_initializer.h"
#include "core/tool_catalog.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>

#define TEST_TOOL_INITIALIZER_TIMEOUT_SECONDS 3

static char *test_tool_initializer_directory = NULL;

typedef struct
{
    guint callback_count;
    guint *observed_callback_count;
    guint *destroy_count;

    BackgroundTaskState final_state;

    gboolean received_summary;
    gboolean received_error;
    gboolean initializer_was_running;

    ToolInitializationSummary summary;

    gsize registry_count;

    char *curl_version;
    GError *error;

    GThread *callback_thread;
} TestToolInitializerCallbackData;

static void test_tool_initializer_callback_data_free(
    gpointer user_data
)
{
    TestToolInitializerCallbackData *callback_data =
        user_data;

    if (callback_data == NULL)
    {
        return;
    }

    if (callback_data->destroy_count != NULL)
    {
        (*callback_data->destroy_count)++;
    }

    g_clear_pointer(
        &callback_data->curl_version,
        g_free
    );

    g_clear_error(
        &callback_data->error
    );

    g_free(
        callback_data
    );
}

static void test_tool_initializer_completed_callback(
    ToolInitializer *tool_initializer,
    BackgroundTaskState final_state,
    const ToolInitializationSummary *summary,
    const GError *error,
    gpointer user_data
)
{
    TestToolInitializerCallbackData *callback_data =
        user_data;

    ToolRegistry *tool_registry = NULL;
    const ToolInfo *curl_info = NULL;

    g_assert_nonnull(
        tool_initializer
    );

    g_assert_nonnull(
        callback_data
    );

    callback_data->callback_count++;

    if (callback_data->observed_callback_count != NULL)
    {
        (*callback_data->observed_callback_count)++;
    }

    callback_data->final_state =
        final_state;

    callback_data->initializer_was_running =
        tool_initializer_is_running(
            tool_initializer
        );

    callback_data->callback_thread =
        g_thread_self();

    callback_data->received_summary =
        summary != NULL;

    if (summary != NULL)
    {
        callback_data->summary =
            *summary;
    }
    else
    {
        callback_data->summary =
            (ToolInitializationSummary) {0};
    }

    callback_data->received_error =
        error != NULL;

    g_clear_error(
        &callback_data->error
    );

    if (error != NULL)
    {
        callback_data->error =
            g_error_copy(
                error
            );
    }

    tool_registry =
        tool_initializer_get_registry(
            tool_initializer
        );

    if (tool_registry != NULL)
    {
        callback_data->registry_count =
            tool_registry_get_count(
                tool_registry
            );

        curl_info =
            tool_registry_find(
                tool_registry,
                "http.curl"
            );
    }

    g_clear_pointer(
        &callback_data->curl_version,
        g_free
    );

    if (curl_info != NULL)
    {
        callback_data->curl_version =
            g_strdup(
                tool_info_get_detected_version(
                    curl_info
                )
            );
    }
}

static gboolean test_tool_initializer_wait_for_progress(
    ToolInitializer *tool_initializer,
    double minimum_progress
)
{
    const gint64 timeout_microseconds =
        3 * G_TIME_SPAN_SECOND;

    gint64 deadline = 0;

    g_assert_nonnull(
        tool_initializer
    );

    deadline =
        g_get_monotonic_time() +
        timeout_microseconds;

    while (g_get_monotonic_time() < deadline)
    {
        BackgroundTask *task = NULL;
        BackgroundTaskState task_state;
        double progress = 0.0;

        task =
            tool_initializer_get_task(
                tool_initializer
            );

        if (task != NULL)
        {
            task_state =
                background_task_get_state(
                    task
                );

            progress =
                background_task_get_progress(
                    task
                );

            background_task_unref(
                task
            );

            if (progress >= minimum_progress)
            {
                return TRUE;
            }

            if (task_state == BACKGROUND_TASK_STATE_COMPLETED ||
                task_state == BACKGROUND_TASK_STATE_FAILED ||
                task_state == BACKGROUND_TASK_STATE_CANCELLED)
            {
                return FALSE;
            }
        }

        while (g_main_context_iteration(
            NULL,
            FALSE
        ))
        {
        }

        g_usleep(
            1000
        );
    }

    return FALSE;
}

static void test_tool_initializer_reset_fake_tools(void)
{
    gsize tool_index = 0;

    for (tool_index = 0;
         tool_index < tool_catalog_get_count();
         tool_index++)
    {
        const ToolCatalogEntry *catalog_entry = NULL;
        const char *executable_name = NULL;
        char *executable_path = NULL;

        catalog_entry =
            tool_catalog_get_entry(
                tool_index
            );

        executable_name =
            tool_catalog_entry_get_executable_name(
                catalog_entry
            );

        if (executable_name == NULL)
        {
            continue;
        }

        executable_path =
            g_build_filename(
                test_tool_initializer_directory,
                executable_name,
                NULL
            );

        g_remove(
            executable_path
        );

        g_free(
            executable_path
        );
    }
}

static void test_tool_initializer_write_fake_tool(
    const char *executable_name,
    const char *script_content
)
{
    char *executable_path = NULL;
    GError *error = NULL;

    g_assert_nonnull(
        test_tool_initializer_directory
    );

    g_assert_nonnull(
        executable_name
    );

    g_assert_nonnull(
        script_content
    );

    executable_path =
        g_build_filename(
            test_tool_initializer_directory,
            executable_name,
            NULL
        );

    g_assert_true(
        g_file_set_contents(
            executable_path,
            script_content,
            -1,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        g_chmod(
            executable_path,
            S_IRUSR |
            S_IWUSR |
            S_IXUSR
        ),
        ==,
        0
    );

    g_free(
        executable_path
    );
}

static gboolean test_tool_initializer_timeout(
    gpointer user_data
)
{
    gboolean *timed_out =
        user_data;

    g_assert_nonnull(
        timed_out
    );

    *timed_out = TRUE;

    return G_SOURCE_REMOVE;
}

static gboolean test_tool_initializer_wait_until_stopped(
    ToolInitializer *tool_initializer
)
{
    gboolean timed_out = FALSE;
    guint timeout_source_id = 0;

    g_assert_nonnull(
        tool_initializer
    );

    timeout_source_id =
        g_timeout_add_seconds(
            TEST_TOOL_INITIALIZER_TIMEOUT_SECONDS,
            test_tool_initializer_timeout,
            &timed_out
        );

    g_assert_cmpuint(
        timeout_source_id,
        !=,
        0
    );

    while (tool_initializer_is_running(
        tool_initializer
    ) &&
           !timed_out)
    {
        g_main_context_iteration(
            NULL,
            TRUE
        );
    }

    if (!timed_out)
    {
        g_assert_true(
            g_source_remove(
                timeout_source_id
            )
        );
    }

    return !timed_out &&
           !tool_initializer_is_running(
               tool_initializer
           );
}

static gboolean test_tool_initializer_wait_for_task_terminal(
    BackgroundTask *task
)
{
    gboolean timed_out = FALSE;
    gboolean is_terminal = FALSE;
    guint timeout_source_id = 0;

    g_assert_nonnull(
        task
    );

    timeout_source_id =
        g_timeout_add_seconds(
            TEST_TOOL_INITIALIZER_TIMEOUT_SECONDS,
            test_tool_initializer_timeout,
            &timed_out
        );

    g_assert_cmpuint(
        timeout_source_id,
        !=,
        0
    );

    while (!timed_out)
    {
        BackgroundTaskState task_state;

        task_state =
            background_task_get_state(
                task
            );

        is_terminal =
            task_state == BACKGROUND_TASK_STATE_COMPLETED ||
            task_state == BACKGROUND_TASK_STATE_FAILED ||
            task_state == BACKGROUND_TASK_STATE_CANCELLED;

        if (is_terminal)
        {
            break;
        }

        g_main_context_iteration(
            NULL,
            TRUE
        );
    }

    if (!timed_out)
    {
        g_assert_true(
            g_source_remove(
                timeout_source_id
            )
        );
    }

    return !timed_out &&
           is_terminal;
}

static void test_tool_initializer_new_invalid_arguments(void)
{
    ToolInitializer *tool_initializer = NULL;
    GError *error = NULL;

    tool_initializer = tool_initializer_new(
        NULL,
        &error
    );

    g_assert_null(
        tool_initializer
    );

    g_assert_error(
        error,
        TOOL_INITIALIZER_ERROR,
        TOOL_INITIALIZER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_initializer = tool_initializer_new(
        NULL,
        NULL
    );

    g_assert_null(
        tool_initializer
    );
}

static void test_tool_initializer_new_valid(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    ToolRegistry *tool_registry = NULL;
    GError *error = NULL;

    task_manager = task_manager_new();

    g_assert_nonnull(
        task_manager
    );

    tool_initializer = tool_initializer_new(
        task_manager,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        tool_initializer
    );

    tool_registry = tool_initializer_get_registry(
        tool_initializer
    );

    g_assert_nonnull(
        tool_registry
    );

    g_assert_cmpuint(
        tool_registry_get_count(
            tool_registry
        ),
        ==,
        0
    );

    g_assert_false(
        tool_initializer_is_running(
            tool_initializer
        )
    );

    g_assert_null(
        tool_initializer_get_task(
            tool_initializer
        )
    );

    tool_initializer_free(
        tool_initializer
    );

    /*
     * ToolInitializer ne possède pas le TaskManager.
     */
    g_assert_cmpuint(
        task_manager_get_count(
            task_manager
        ),
        ==,
        0
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_null_accessors(void)
{
    ToolInitializationSummary summary = {0};

    g_assert_null(
        tool_initializer_get_registry(
            NULL
        )
    );

    g_assert_false(
        tool_initializer_is_running(
            NULL
        )
    );

    g_assert_null(
        tool_initializer_get_task(
            NULL
        )
    );

    g_assert_false(
        tool_initializer_get_last_summary(
            NULL,
            &summary
        )
    );

    tool_initializer_free(
        NULL
    );
}

static void test_tool_initializer_registry_is_owned(void)
{
    TaskManager *task_manager = NULL;

    ToolInitializer *first_initializer = NULL;
    ToolInitializer *second_initializer = NULL;

    ToolRegistry *first_registry = NULL;
    ToolRegistry *second_registry = NULL;

    GError *error = NULL;

    task_manager = task_manager_new();

    g_assert_nonnull(
        task_manager
    );

    first_initializer = tool_initializer_new(
        task_manager,
        &error
    );

    g_assert_no_error(
        error
    );

    second_initializer = tool_initializer_new(
        task_manager,
        &error
    );

    g_assert_no_error(
        error
    );

    first_registry = tool_initializer_get_registry(
        first_initializer
    );

    second_registry = tool_initializer_get_registry(
        second_initializer
    );

    g_assert_nonnull(
        first_registry
    );

    g_assert_nonnull(
        second_registry
    );

    /*
     * Chaque initialiseur possède son propre registre.
     */
    g_assert_true(
        first_registry != second_registry
    );

    tool_initializer_free(
        first_initializer
    );

    tool_initializer_free(
        second_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_start_invalid_arguments(void)
{
    TaskManager *task_manager = NULL;

    ToolInitializer *tool_initializer = NULL;
    GError *error = NULL;

    g_assert_false(
        tool_initializer_start(
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_INITIALIZER_ERROR,
        TOOL_INITIALIZER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    task_manager = task_manager_new();

    tool_initializer = tool_initializer_new(
        task_manager,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_false(
        tool_initializer_cancel(
            tool_initializer,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_INITIALIZER_ERROR,
        TOOL_INITIALIZER_ERROR_NOT_RUNNING
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_initializer_cancel(
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_INITIALIZER_ERROR,
        TOOL_INITIALIZER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_start(void)
{
    test_tool_initializer_reset_fake_tools();

    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    BackgroundTask *task = NULL;

    ToolInitializationSummary summary = {0};

    GError *error = NULL;

    task_manager = task_manager_new();

    tool_initializer = tool_initializer_new(
        task_manager,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_false(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_initializer_is_running(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        task_manager_get_count(
            task_manager
        ),
        ==,
        1
    );

    task = tool_initializer_get_task(
        tool_initializer
    );

    g_assert_nonnull(
        task
    );

    g_assert_cmpstr(
        background_task_get_title(
            task
        ),
        ==,
        "Initialisation des outils externes"
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpint(
        background_task_get_state(
            task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_assert_true(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    g_assert_cmpuint(
        summary.total_count,
        ==,
        tool_catalog_get_count()
    );

    g_assert_cmpuint(
        summary.available_count,
        ==,
        0
    );

    g_assert_cmpuint(
        summary.missing_count,
        ==,
        tool_catalog_get_count()
    );

    g_assert_cmpuint(
        summary.version_detected_count,
        ==,
        0
    );

    g_assert_cmpuint(
        summary.version_failed_count,
        ==,
        0
    );

    g_assert_cmpuint(
        tool_registry_get_count(
            tool_initializer_get_registry(
                tool_initializer
            )
        ),
        ==,
        tool_catalog_get_count()
    );

    g_assert_cmpfloat(
        background_task_get_progress(
            task
        ),
        ==,
        1.0
    );

    background_task_unref(
        task
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_double_start(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    GError *error = NULL;

    task_manager = task_manager_new();

    tool_initializer = tool_initializer_new(
        task_manager,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    /*
     * Le callback de fin n’a pas encore été distribué par la boucle
     * principale : l’initialiseur est donc toujours considéré actif.
     */
    g_assert_false(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_INITIALIZER_ERROR,
        TOOL_INITIALIZER_ERROR_ALREADY_RUNNING
    );

    g_clear_error(
        &error
    );

    g_assert_cmpuint(
        task_manager_get_count(
            task_manager
        ),
        ==,
        1
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_detect_versions(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    ToolRegistry *tool_registry = NULL;
    BackgroundTask *task = NULL;

    const ToolInfo *curl_info = NULL;
    const ToolInfo *openssl_info = NULL;

    ToolInitializationSummary summary = {0};

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "curl",
        "#!/bin/sh\n"
        "printf 'Fake curl 1.2.3\\n'\n"
    );

    test_tool_initializer_write_fake_tool(
        "openssl",
        "#!/bin/sh\n"
        "printf 'Fake OpenSSL 3.0.0\\n' >&2\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    task =
        tool_initializer_get_task(
            tool_initializer
        );

    g_assert_nonnull(
        task
    );

    g_assert_cmpint(
        background_task_get_state(
            task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_assert_true(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    g_assert_cmpuint(
        summary.total_count,
        ==,
        5
    );

    g_assert_cmpuint(
        summary.available_count,
        ==,
        2
    );

    g_assert_cmpuint(
        summary.missing_count,
        ==,
        3
    );

    g_assert_cmpuint(
        summary.version_detected_count,
        ==,
        2
    );

    g_assert_cmpuint(
        summary.version_failed_count,
        ==,
        0
    );

    tool_registry =
        tool_initializer_get_registry(
            tool_initializer
        );

    curl_info =
        tool_registry_find(
            tool_registry,
            "http.curl"
        );

    openssl_info =
        tool_registry_find(
            tool_registry,
            "tls.openssl"
        );

    g_assert_nonnull(
        curl_info
    );

    g_assert_nonnull(
        openssl_info
    );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            curl_info
        ),
        ==,
        "Fake curl 1.2.3"
    );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            openssl_info
        ),
        ==,
        "Fake OpenSSL 3.0.0"
    );

    background_task_unref(
        task
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_version_failure_is_nonfatal(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    ToolRegistry *tool_registry = NULL;
    BackgroundTask *task = NULL;

    const ToolInfo *curl_info = NULL;
    const ToolInfo *openssl_info = NULL;

    ToolInitializationSummary summary = {0};

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "curl",
        "#!/bin/sh\n"
        "printf 'Version inutilisable\\n'\n"
        "exit 7\n"
    );

    test_tool_initializer_write_fake_tool(
        "openssl",
        "#!/bin/sh\n"
        "printf 'Fake OpenSSL 4.0.0\\n'\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    task =
        tool_initializer_get_task(
            tool_initializer
        );

    g_assert_nonnull(
        task
    );

    g_assert_cmpint(
        background_task_get_state(
            task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_assert_true(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    g_assert_cmpuint(
        summary.available_count,
        ==,
        2
    );

    g_assert_cmpuint(
        summary.missing_count,
        ==,
        3
    );

    g_assert_cmpuint(
        summary.version_detected_count,
        ==,
        1
    );

    g_assert_cmpuint(
        summary.version_failed_count,
        ==,
        1
    );

    tool_registry =
        tool_initializer_get_registry(
            tool_initializer
        );

    curl_info =
        tool_registry_find(
            tool_registry,
            "http.curl"
        );

    openssl_info =
        tool_registry_find(
            tool_registry,
            "tls.openssl"
        );

    g_assert_null(
        tool_info_get_detected_version(
            curl_info
        )
    );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            openssl_info
        ),
        ==,
        "Fake OpenSSL 4.0.0"
    );

    background_task_unref(
        task
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_cancel_running(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    BackgroundTask *task = NULL;

    ToolInitializationSummary summary = {0};

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    /*
     * dig est le premier outil du catalogue.
     *
     * Le processus reste actif suffisamment longtemps pour permettre
     * de tester l’annulation de ToolProcess.
     */
    test_tool_initializer_write_fake_tool(
        "dig",
        "#!/bin/sh\n"
        "exec /bin/sleep 30\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_for_progress(
            tool_initializer,
            0.15
        )
    );

    task =
        tool_initializer_get_task(
            tool_initializer
        );

    g_assert_nonnull(
        task
    );

    g_assert_true(
        tool_initializer_cancel(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpint(
        background_task_get_state(
            task
        ),
        ==,
        BACKGROUND_TASK_STATE_CANCELLED
    );

    /*
     * Une initialisation annulée ne produit pas de résumé valide.
     */
    g_assert_false(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    background_task_unref(
        task
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_restart_after_completion(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    ToolRegistry *tool_registry = NULL;

    const ToolInfo *curl_info = NULL;

    ToolInitializationSummary summary = {0};

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "curl",
        "#!/bin/sh\n"
        "printf 'Fake curl 1.0.0\\n'\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    tool_registry =
        tool_initializer_get_registry(
            tool_initializer
        );

    curl_info =
        tool_registry_find(
            tool_registry,
            "http.curl"
        );

    g_assert_nonnull(
        curl_info
    );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            curl_info
        ),
        ==,
        "Fake curl 1.0.0"
    );

    test_tool_initializer_write_fake_tool(
        "curl",
        "#!/bin/sh\n"
        "/bin/sleep 1\n"
        "printf 'Fake curl 2.0.0\\n'\n"
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    /*
     * Le résumé précédent n’est plus valide pendant cette nouvelle
     * exécution.
     */
    g_assert_false(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    curl_info =
        tool_registry_find(
            tool_registry,
            "http.curl"
        );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            curl_info
        ),
        ==,
        "Fake curl 2.0.0"
    );

    g_assert_true(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    g_assert_cmpuint(
        summary.available_count,
        ==,
        1
    );

    g_assert_cmpuint(
        summary.missing_count,
        ==,
        tool_catalog_get_count() - 1
    );

    /*
     * TaskManager conserve les deux tâches terminées.
     */
    g_assert_cmpuint(
        task_manager_get_count(
            task_manager
        ),
        ==,
        2
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_restart_after_cancellation(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    ToolRegistry *tool_registry = NULL;

    const ToolInfo *curl_info = NULL;

    ToolInitializationSummary summary = {0};

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "dig",
        "#!/bin/sh\n"
        "exec /bin/sleep 30\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_true(
        tool_initializer_is_running(
            tool_initializer
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_for_progress(
            tool_initializer,
            0.15
        )
    );

    g_assert_true(
        tool_initializer_cancel(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_false(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    /*
     * Le second démarrage utilise un environnement différent.
     */
    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "curl",
        "#!/bin/sh\n"
        "printf 'Fake curl 3.0.0\\n'\n"
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_true(
        tool_initializer_get_last_summary(
            tool_initializer,
            &summary
        )
    );

    g_assert_cmpuint(
        summary.available_count,
        ==,
        1
    );

    g_assert_cmpuint(
        summary.version_detected_count,
        ==,
        1
    );

    tool_registry =
        tool_initializer_get_registry(
            tool_initializer
        );

    curl_info =
        tool_registry_find(
            tool_registry,
            "http.curl"
        );

    g_assert_nonnull(
        curl_info
    );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            curl_info
        ),
        ==,
        "Fake curl 3.0.0"
    );

    g_assert_cmpuint(
        task_manager_get_count(
            task_manager
        ),
        ==,
        2
    );

    tool_initializer_free(
        tool_initializer
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_success(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    guint destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        tool_initializer
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        callback_data->callback_count,
        ==,
        1
    );

    g_assert_cmpint(
        callback_data->final_state,
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_assert_false(
        callback_data->initializer_was_running
    );

    g_assert_true(
        callback_data->received_summary
    );

    g_assert_false(
        callback_data->received_error
    );

    g_assert_null(
        callback_data->error
    );

    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_summary_and_registry(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    guint destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "curl",
        "#!/bin/sh\n"
        "printf 'Fake curl 5.4.3\\n'\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        callback_data->callback_count,
        ==,
        1
    );

    g_assert_true(
        callback_data->received_summary
    );

    g_assert_cmpuint(
        callback_data->summary.total_count,
        ==,
        tool_catalog_get_count()
    );

    g_assert_cmpuint(
        callback_data->summary.available_count,
        ==,
        1
    );

    g_assert_cmpuint(
        callback_data->summary.missing_count,
        ==,
        tool_catalog_get_count() - 1
    );

    g_assert_cmpuint(
        callback_data->summary.version_detected_count,
        ==,
        1
    );

    g_assert_cmpuint(
        callback_data->summary.version_failed_count,
        ==,
        0
    );

    /*
     * Le catalogue et les versions doivent déjà être enregistrés
     * lorsque le callback est exécuté.
     */
    g_assert_cmpuint(
        callback_data->registry_count,
        ==,
        tool_catalog_get_count()
    );

    g_assert_cmpstr(
        callback_data->curl_version,
        ==,
        "Fake curl 5.4.3"
    );

    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_replacement(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;

    TestToolInitializerCallbackData *first_callback_data = NULL;
    TestToolInitializerCallbackData *second_callback_data = NULL;

    guint first_callback_count = 0;
    guint second_callback_count = 0;

    guint first_destroy_count = 0;
    guint second_destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    first_callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    first_callback_data->observed_callback_count =
        &first_callback_count;

    first_callback_data->destroy_count =
        &first_destroy_count;

    second_callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    second_callback_data->observed_callback_count =
        &second_callback_count;

    second_callback_data->destroy_count =
        &second_destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        first_callback_data,
        test_tool_initializer_callback_data_free
    );

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        second_callback_data,
        test_tool_initializer_callback_data_free
    );

    /*
     * Le premier enregistrement a été remplacé et doit déjà être
     * détruit.
     */
    g_assert_cmpuint(
        first_destroy_count,
        ==,
        1
    );

    g_assert_cmpuint(
        second_destroy_count,
        ==,
        0
    );

    g_assert_cmpuint(
        first_callback_count,
        ==,
        0
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        first_callback_count,
        ==,
        0
    );

    g_assert_cmpuint(
        second_callback_count,
        ==,
        1
    );

    g_assert_cmpuint(
        second_callback_data->callback_count,
        ==,
        1
    );

    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        first_destroy_count,
        ==,
        1
    );

    g_assert_cmpuint(
        second_destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_destroy_notify(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    guint destroy_count = 0;

    GError *error = NULL;

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        0
    );

    tool_initializer_set_completed_callback(
        tool_initializer,
        NULL,
        NULL,
        NULL
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    /*
     * La destruction de l’initialiseur ne doit pas appeler une seconde
     * fois le destroy notify de l’ancien enregistrement.
     */
    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_cancellation(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    guint destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "dig",
        "#!/bin/sh\n"
        "exec /bin/sleep 30\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_for_progress(
            tool_initializer,
            0.15
        )
    );

    g_assert_true(
        tool_initializer_cancel(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        callback_data->callback_count,
        ==,
        1
    );

    g_assert_cmpint(
        callback_data->final_state,
        ==,
        BACKGROUND_TASK_STATE_CANCELLED
    );

    g_assert_false(
        callback_data->received_summary
    );

    g_assert_false(
        callback_data->received_error
    );

    g_assert_false(
        callback_data->initializer_was_running
    );

    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_failure(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    ToolRegistry *tool_registry = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    guint destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    tool_registry =
        tool_initializer_get_registry(
            tool_initializer
        );

    g_assert_nonnull(
        tool_registry
    );

    /*
     * Le registre ne correspondra plus exactement au catalogue.
     * Le worker doit donc échouer pendant sa vérification initiale.
     */
    g_assert_true(
        tool_registry_register(
            tool_registry,
            "test.extra",
            "Outil supplémentaire",
            "extra",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        callback_data->callback_count,
        ==,
        1
    );

    g_assert_cmpint(
        callback_data->final_state,
        ==,
        BACKGROUND_TASK_STATE_FAILED
    );

    g_assert_false(
        callback_data->received_summary
    );

    g_assert_true(
        callback_data->received_error
    );

    g_assert_nonnull(
        callback_data->error
    );

    g_assert_error(
        callback_data->error,
        TOOL_INITIALIZER_ERROR,
        TOOL_INITIALIZER_ERROR_CATALOG_REGISTRATION
    );

    g_assert_false(
        callback_data->initializer_was_running
    );

    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_main_context(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    GThread *main_thread = NULL;

    guint destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    main_thread =
        g_thread_self();

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        callback_data->callback_count,
        ==,
        1
    );

    g_assert_true(
        callback_data->callback_thread ==
        main_thread
    );

    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_callback_restart(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    guint destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        callback_data->callback_count,
        ==,
        1
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_until_stopped(
            tool_initializer
        )
    );

    g_assert_cmpuint(
        callback_data->callback_count,
        ==,
        2
    );

    g_assert_cmpint(
        callback_data->final_state,
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_assert_cmpuint(
        task_manager_get_count(
            task_manager
        ),
        ==,
        2
    );

    tool_initializer_free(
        tool_initializer
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    task_manager_free(
        task_manager
    );
}

static void test_tool_initializer_free_while_running(void)
{
    TaskManager *task_manager = NULL;
    ToolInitializer *tool_initializer = NULL;
    BackgroundTask *task = NULL;

    TestToolInitializerCallbackData *callback_data = NULL;

    guint callback_count = 0;
    guint destroy_count = 0;

    GError *error = NULL;

    test_tool_initializer_reset_fake_tools();

    test_tool_initializer_write_fake_tool(
        "dig",
        "#!/bin/sh\n"
        "exec /bin/sleep 30\n"
    );

    task_manager =
        task_manager_new();

    tool_initializer =
        tool_initializer_new(
            task_manager,
            &error
        );

    g_assert_no_error(
        error
    );

    callback_data =
        g_new0(
            TestToolInitializerCallbackData,
            1
        );

    callback_data->observed_callback_count =
        &callback_count;

    callback_data->destroy_count =
        &destroy_count;

    tool_initializer_set_completed_callback(
        tool_initializer,
        test_tool_initializer_completed_callback,
        callback_data,
        test_tool_initializer_callback_data_free
    );

    g_assert_true(
        tool_initializer_start(
            tool_initializer,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        test_tool_initializer_wait_for_progress(
            tool_initializer,
            0.15
        )
    );

    task =
        tool_initializer_get_task(
            tool_initializer
        );

    g_assert_nonnull(
        task
    );

    /*
     * La libération publique supprime le callback et demande
     * l’annulation, sans attendre la fin du worker.
     */
    tool_initializer_free(
        tool_initializer
    );

    tool_initializer = NULL;

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    g_assert_cmpuint(
        callback_count,
        ==,
        0
    );

    g_assert_true(
        test_tool_initializer_wait_for_task_terminal(
            task
        )
    );

    g_assert_cmpint(
        background_task_get_state(
            task
        ),
        ==,
        BACKGROUND_TASK_STATE_CANCELLED
    );

    /*
     * Aucun callback utilisateur ne doit être déclenché après free().
     */
    g_assert_cmpuint(
        callback_count,
        ==,
        0
    );

    g_assert_cmpuint(
        destroy_count,
        ==,
        1
    );

    background_task_unref(
        task
    );

    task_manager_free(
        task_manager
    );
}

int main(
    int argc,
    char **argv
)
{
    g_test_init(
        &argc,
        &argv,
        NULL
    );

    {
        GError *error = NULL;

        test_tool_initializer_directory =
            g_dir_make_tmp(
                "labfy-tool-initializer-XXXXXX",
                &error
            );

        g_assert_no_error(
            error
        );

        g_assert_nonnull(
            test_tool_initializer_directory
        );

        g_assert_true(
            g_setenv(
                "PATH",
                test_tool_initializer_directory,
                TRUE
            )
        );
    }

    g_test_add_func(
        "/tool_initializer/detect_versions",
        test_tool_initializer_detect_versions
    );

    g_test_add_func(
        "/tool_initializer/version_failure_is_nonfatal",
        test_tool_initializer_version_failure_is_nonfatal
    );

    g_test_add_func(
        "/tool_initializer/new_invalid_arguments",
        test_tool_initializer_new_invalid_arguments
    );

    g_test_add_func(
        "/tool_initializer/new_valid",
        test_tool_initializer_new_valid
    );

    g_test_add_func(
        "/tool_initializer/null_accessors",
        test_tool_initializer_null_accessors
    );

    g_test_add_func(
        "/tool_initializer/registry_is_owned",
        test_tool_initializer_registry_is_owned
    );

    g_test_add_func(
        "/tool_initializer/start_invalid_arguments",
        test_tool_initializer_start_invalid_arguments
    );

    g_test_add_func(
        "/tool_initializer/start",
        test_tool_initializer_start
    );

    g_test_add_func(
        "/tool_initializer/double_start",
        test_tool_initializer_double_start
    );

    g_test_add_func(
        "/tool_initializer/cancel_running",
        test_tool_initializer_cancel_running
    );

    g_test_add_func(
        "/tool_initializer/restart_after_completion",
        test_tool_initializer_restart_after_completion
    );

    g_test_add_func(
        "/tool_initializer/restart_after_cancellation",
        test_tool_initializer_restart_after_cancellation
    );

    g_test_add_func(
        "/tool_initializer/callback_success",
        test_tool_initializer_callback_success
    );

    g_test_add_func(
        "/tool_initializer/callback_summary_and_registry",
        test_tool_initializer_callback_summary_and_registry
    );

    g_test_add_func(
        "/tool_initializer/callback_replacement",
        test_tool_initializer_callback_replacement
    );

    g_test_add_func(
        "/tool_initializer/callback_destroy_notify",
        test_tool_initializer_callback_destroy_notify
    );

    g_test_add_func(
        "/tool_initializer/callback_cancellation",
        test_tool_initializer_callback_cancellation
    );

    g_test_add_func(
        "/tool_initializer/callback_failure",
        test_tool_initializer_callback_failure
    );

    g_test_add_func(
        "/tool_initializer/callback_main_context",
        test_tool_initializer_callback_main_context
    );

    g_test_add_func(
        "/tool_initializer/callback_restart",
        test_tool_initializer_callback_restart
    );

    g_test_add_func(
        "/tool_initializer/free_while_running",
        test_tool_initializer_free_while_running
    );
    {
        int test_result = 0;

        test_result =
            g_test_run();

        test_tool_initializer_reset_fake_tools();

        g_rmdir(
            test_tool_initializer_directory
        );

        g_clear_pointer(
            &test_tool_initializer_directory,
            g_free
        );

        return test_result;
    }
}
