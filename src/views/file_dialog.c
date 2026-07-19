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
