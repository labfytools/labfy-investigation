#include "views/identity_ocr_language_loader.h"
#include <gio/gio.h>

typedef struct {
    char *executable;
    GWeakRef window;
    GWeakRef dropdown;
} IdentityLanguageJob;

static void identity_language_job_free(gpointer data)
{
    IdentityLanguageJob *job = data;
    if (job != NULL) {
        g_free(job->executable);
        g_weak_ref_clear(&job->window);
        g_weak_ref_clear(&job->dropdown);
        g_free(job);
    }
}

static void identity_languages_worker(GTask *task, gpointer source,
    gpointer task_data, GCancellable *cancellable)
{
    (void)source; (void)cancellable;
    IdentityLanguageJob *job = task_data;
    gchar *stdout_text = NULL;
    gboolean success = g_spawn_command_line_sync(
        g_strdup_printf("'%s' --list-langs", job->executable),
        &stdout_text, NULL, NULL, NULL);
    if (success && stdout_text != NULL) {
        GPtrArray *langs = g_ptr_array_new_with_free_func(g_free);
        gchar **lines = g_strsplit(stdout_text, "\n", -1);
        for (gsize i = 1; lines[i] != NULL; i++) {
            gchar *lang = g_strstrip(g_strdup(lines[i]));
            if (lang[0] != '\0') g_ptr_array_add(langs, lang);
            else g_free(lang);
        }
        g_strfreev(lines);
        g_task_return_pointer(task, langs, (GDestroyNotify)g_ptr_array_unref);
    } else {
        g_task_return_pointer(task, NULL, NULL);
    }
    g_free(stdout_text);
}

static void identity_languages_completed(GObject *source,
    GAsyncResult *result, gpointer data)
{
    (void)source;
    GTask *task = G_TASK(result);
    IdentityLanguageJob *job = data;
    GPtrArray *langs = g_task_propagate_pointer(task, NULL);
    if (langs != NULL) {
        GtkWindow *window = GTK_WINDOW(g_weak_ref_get(&job->window));
        GtkDropDown *dropdown = GTK_DROP_DOWN(g_weak_ref_get(&job->dropdown));
        if (window != NULL && dropdown != NULL) {
            GtkStringList *list = gtk_string_list_new(NULL);
            gtk_string_list_append(list, "Fra (français)");
            for (guint i = 0; i < langs->len; i++) {
                const char *lang = g_ptr_array_index(langs, i);
                if (g_strcmp0(lang, "fra") != 0) {
                    gtk_string_list_append(list, lang);
                }
            }
            gtk_drop_down_set_model(dropdown, G_LIST_MODEL(list));
        }
        if (window != NULL) g_object_unref(window);
        if (dropdown != NULL) g_object_unref(dropdown);
        g_ptr_array_unref(langs);
    }
}

void identity_ocr_language_loader_start(GtkWindow *window, const char *tesseract_path, GtkDropDown *language_dropdown)
{
    if (tesseract_path == NULL || window == NULL || language_dropdown == NULL) return;
    IdentityLanguageJob *job = g_new0(IdentityLanguageJob, 1);
    job->executable = g_strdup(tesseract_path);
    g_weak_ref_init(&job->window, G_OBJECT(window));
    g_weak_ref_init(&job->dropdown, G_OBJECT(language_dropdown));
    GTask *task = g_task_new(NULL, NULL, identity_languages_completed, job);
    g_task_set_task_data(task, job, identity_language_job_free);
    g_task_run_in_thread(task, identity_languages_worker);
    g_object_unref(task);
}
