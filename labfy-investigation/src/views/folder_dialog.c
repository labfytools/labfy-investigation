/******************************************************************************
 * @file folder_dialog.c
 * @brief Implémentation du dialogue de sélection d'un dossier d'enquête.
 ******************************************************************************/

#include "views/folder_dialog.h"

#include <gio/gio.h>

/**
 * @struct FolderDialogContext
 * @brief Conserve les données nécessaires pendant l'opération asynchrone.
 *
 * Le dialogue GTK ne se termine pas immédiatement. Ce contexte permet donc
 * de conserver le callback et ses données jusqu'à la fermeture du dialogue.
 */
typedef struct
{
    FolderDialogCallback callback;
    gpointer user_data;
} FolderDialogContext;

/**
 * @brief Libère le contexte associé au dialogue.
 *
 * @param context Contexte à libérer.
 */
static void folder_dialog_context_free(FolderDialogContext *context)
{
    g_free(context);
}

/**
 * @brief Traite le résultat de la sélection du dossier.
 *
 * @param source_object Objet ayant lancé l'opération asynchrone.
 * @param result        Résultat transmis par GTK.
 * @param user_data     Contexte du dialogue.
 */
static void folder_dialog_on_folder_selected(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data
)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    FolderDialogContext *context = user_data;
    GError *error = NULL;
    GFile *folder = NULL;
    char *folder_path = NULL;

    folder = gtk_file_dialog_select_folder_finish(dialog, result, &error);

    if (folder == NULL)
    {
        /*
         * Une annulation est un comportement normal de l'utilisateur.
         * Elle est transmise au code appelant sous la forme d'un chemin NULL.
         */
        if (error != NULL &&
            !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        {
            g_warning(
                "Impossible de sélectionner le dossier : %s",
                error->message
            );
        }

        if (context->callback != NULL)
        {
            context->callback(NULL, context->user_data);
        }

        g_clear_error(&error);
        folder_dialog_context_free(context);
        return;
    }

    folder_path = g_file_get_path(folder);

    if (context->callback != NULL)
    {
        context->callback(folder_path, context->user_data);
    }

    g_free(folder_path);
    g_object_unref(folder);
    folder_dialog_context_free(context);
}

void folder_dialog_select_folder(
    GtkWindow *parent,
    FolderDialogCallback callback,
    gpointer user_data
)
{
    GtkFileDialog *dialog = NULL;
    FolderDialogContext *context = NULL;

    context = g_new0(FolderDialogContext, 1);
    context->callback = callback;
    context->user_data = user_data;

    dialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(
        dialog,
        "Sélectionner un dossier d'enquête"
    );

    gtk_file_dialog_set_modal(dialog, TRUE);

    gtk_file_dialog_select_folder(
        dialog,
        parent,
        NULL,
        folder_dialog_on_folder_selected,
        context
    );

    g_object_unref(dialog);
}
