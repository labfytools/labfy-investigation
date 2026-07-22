/******************************************************************************
 * @file file_dialog.c
 * @brief Implémentation du dialogue de sélection d’un fichier de preuve.
 ******************************************************************************/

#include "views/file_dialog.h"

#include <gio/gio.h>

/**
 * @brief Contexte conservé pendant l’opération asynchrone.
 */
typedef struct
{
    FileDialogCallback callback;
    gpointer user_data;
} FileDialogContext;

/** @brief Contexte conservé pendant une sélection multiple. */
typedef struct
{
    FileDialogMultipleCallback callback;
    gpointer user_data;
} FileDialogMultipleContext;

/**
 * @brief Libère le contexte du dialogue.
 */
static void file_dialog_context_free(
    FileDialogContext *context
)
{
    g_free(
        context
    );
}

/**
 * @brief Transmet le résultat au code appelant.
 */
static void file_dialog_emit_result(
    FileDialogContext *context,
    const char *file_path,
    const GError *error
)
{
    if (context == NULL ||
        context->callback == NULL)
    {
        return;
    }

    context->callback(
        file_path,
        error,
        context->user_data
    );
}

/**
 * @brief Traite le résultat du sélecteur de fichier.
 */
static void file_dialog_on_file_selected(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data
)
{
    GtkFileDialog *dialog =
        GTK_FILE_DIALOG(
            source_object
        );

    FileDialogContext *context =
        user_data;

    GFile *file = NULL;

    char *file_path = NULL;

    GError *error = NULL;
    GError *path_error = NULL;

    if (context == NULL)
    {
        return;
    }

    file =
        gtk_file_dialog_open_finish(
            dialog,
            result,
            &error
        );

    if (file == NULL)
    {
        /*
         * Fermer volontairement le dialogue n’est pas une erreur.
         */
        if (error != NULL &&
            g_error_matches(
                error,
                GTK_DIALOG_ERROR,
                GTK_DIALOG_ERROR_DISMISSED
            ))
        {
            file_dialog_emit_result(
                context,
                NULL,
                NULL
            );
        }
        else
        {
            file_dialog_emit_result(
                context,
                NULL,
                error
            );
        }

        g_clear_error(
            &error
        );

        file_dialog_context_free(
            context
        );

        return;
    }

    file_path =
        g_file_get_path(
            file
        );

    /*
     * g_file_get_path() retourne NULL pour une ressource qui ne
     * correspond pas à un fichier local.
     */
    if (file_path == NULL)
    {
        g_set_error_literal(
            &path_error,
            G_IO_ERROR,
            G_IO_ERROR_NOT_SUPPORTED,
            "Le fichier sélectionné ne possède pas de chemin local."
        );

        file_dialog_emit_result(
            context,
            NULL,
            path_error
        );

        g_clear_error(
            &path_error
        );

        g_object_unref(
            file
        );

        file_dialog_context_free(
            context
        );

        return;
    }

    file_dialog_emit_result(
        context,
        file_path,
        NULL
    );

    g_free(
        file_path
    );

    g_object_unref(
        file
    );

    file_dialog_context_free(
        context
    );
}

void file_dialog_select_file(
    GtkWindow *parent,
    FileDialogCallback callback,
    gpointer user_data
)
{
    GtkFileDialog *dialog = NULL;

    FileDialogContext *context = NULL;

    if (callback == NULL)
    {
        return;
    }

    context =
        g_new0(
            FileDialogContext,
            1
        );

    context->callback =
        callback;

    context->user_data =
        user_data;

    dialog =
        gtk_file_dialog_new();

    gtk_file_dialog_set_title(
        dialog,
        "Sélectionner un fichier de preuve"
    );

    gtk_file_dialog_set_modal(
        dialog,
        TRUE
    );

    gtk_file_dialog_open(
        dialog,
        parent,
        NULL,
        file_dialog_on_file_selected,
        context
    );

    g_object_unref(
        dialog
    );
}

/** @brief Traite le résultat du sélecteur multiple. */
static void file_dialog_on_files_selected(
    GObject *source_object, GAsyncResult *result, gpointer user_data
)
{
    FileDialogMultipleContext *context = user_data;
    GListModel *files = NULL;
    GPtrArray *paths = NULL;
    GError *error = NULL;
    files = gtk_file_dialog_open_multiple_finish(
        GTK_FILE_DIALOG(source_object), result, &error);
    if (files == NULL)
    {
        if (error != NULL && g_error_matches(
                error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            context->callback(NULL, NULL, context->user_data);
        else context->callback(NULL, error, context->user_data);
        g_clear_error(&error); g_free(context); return;
    }
    paths = g_ptr_array_new_with_free_func(g_free);
    for (guint index = 0U; index < g_list_model_get_n_items(files); index++)
    {
        GFile *file = g_list_model_get_item(files, index);
        char *path = g_file_get_path(file);
        if (path == NULL)
        {
            g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                "Un fichier sélectionné ne possède pas de chemin local.");
            g_object_unref(file); break;
        }
        g_ptr_array_add(paths, path); g_object_unref(file);
    }
    if (error != NULL) context->callback(NULL, error, context->user_data);
    else context->callback(paths, NULL, context->user_data);
    g_clear_error(&error); g_ptr_array_unref(paths); g_object_unref(files);
    g_free(context);
}

void file_dialog_select_files(
    GtkWindow *parent, FileDialogMultipleCallback callback, gpointer user_data
)
{
    GtkFileDialog *dialog = NULL;
    FileDialogMultipleContext *context = NULL;
    if (callback == NULL) return;
    context = g_new0(FileDialogMultipleContext, 1);
    context->callback = callback; context->user_data = user_data;
    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Sélectionner des fichiers de preuve");
    gtk_file_dialog_set_modal(dialog, TRUE);
    gtk_file_dialog_open_multiple(dialog, parent, NULL,
        file_dialog_on_files_selected, context);
    g_object_unref(dialog);
}
