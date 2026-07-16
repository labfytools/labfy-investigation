/******************************************************************************
 * @file create_investigation_dialog.c
 * @brief Dialogue GTK de création d'une nouvelle enquête.
 ******************************************************************************/

#include "views/create_investigation_dialog.h"

#include <gio/gio.h>
#include <glib.h>

/**
 * @brief Largeur initiale du dialogue.
 */
#define CREATE_INVESTIGATION_DIALOG_WIDTH 620

/**
 * @brief Marge extérieure du dialogue.
 */
#define CREATE_INVESTIGATION_DIALOG_MARGIN 16

/**
 * @brief Espacement entre les composants principaux.
 */
#define CREATE_INVESTIGATION_DIALOG_SPACING 12

/**
 * @brief Contexte conservé pendant la durée de vie du dialogue.
 */
typedef struct
{
    GtkWindow *window;

    GtkWidget *parent_directory_entry;
    GtkWidget *investigation_name_entry;
    GtkWidget *create_button;

    char *parent_directory;

    CreateInvestigationDialogCallback callback;
    gpointer user_data;

    gboolean completed;
} CreateInvestigationDialogContext;

/**
 * @brief Libère le contexte associé au dialogue.
 *
 * Les widgets GTK sont possédés par la fenêtre et ne sont donc pas
 * libérés directement ici.
 *
 * @param context Contexte à libérer.
 */
static void create_investigation_dialog_context_free(
    CreateInvestigationDialogContext *context
)
{
    if (context == NULL)
    {
        return;
    }

    g_free(context->parent_directory);
    g_free(context);
}

/**
 * @brief Valide et normalise un nom d'enquête.
 *
 * La fonction :
 *
 * - vérifie que la chaîne est un UTF-8 valide ;
 * - refuse les noms constitués uniquement d'espaces ;
 * - reconnaît les espaces Unicode ;
 * - refuse les séparateurs '/' et '\' ;
 * - retire les espaces placés au début et à la fin.
 *
 * Si normalized_name n'est pas NULL, la fonction y place une nouvelle
 * chaîne allouée que l'appelant devra libérer avec g_free().
 *
 * @param investigation_name Nom à vérifier.
 * @param normalized_name Adresse recevant le nom normalisé, ou NULL.
 *
 * @return TRUE si le nom est valide, sinon FALSE.
 */
static gboolean create_investigation_dialog_normalize_name(
    const char *investigation_name,
    char **normalized_name
)
{
    const char *cursor = NULL;
    const char *first_character = NULL;
    const char *end_after_last_character = NULL;

    if (normalized_name != NULL)
    {
        *normalized_name = NULL;
    }

    if (investigation_name == NULL ||
        !g_utf8_validate(
            investigation_name,
            -1,
            NULL
        ))
    {
        return FALSE;
    }

    cursor = investigation_name;

    while (*cursor != '\0')
    {
        gunichar character = 0;
        const char *next_character = NULL;

        character = g_utf8_get_char(
            cursor
        );

        next_character = g_utf8_next_char(
            cursor
        );

        if (character == '/' ||
            character == '\\')
        {
            return FALSE;
        }

        if (!g_unichar_isspace(character))
        {
            if (first_character == NULL)
            {
                first_character = cursor;
            }

            end_after_last_character = next_character;
        }

        cursor = next_character;
    }

    /*
     * Aucun caractère visible n'a été trouvé.
     * Le nom était vide ou uniquement composé d'espaces.
     */
    if (first_character == NULL ||
        end_after_last_character == NULL)
    {
        return FALSE;
    }

    if (normalized_name != NULL)
    {
        *normalized_name = g_strndup(
            first_character,
            (gsize) (
                end_after_last_character -
                first_character
            )
        );

        if (*normalized_name == NULL)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Met à jour l'état du bouton de création.
 *
 * Le bouton est actif uniquement lorsqu'un dossier parent valide a été
 * sélectionné et que le nom saisi est acceptable.
 *
 * @param context Contexte du dialogue.
 */
static void create_investigation_dialog_update_create_button(
    CreateInvestigationDialogContext *context
)
{
    const char *investigation_name = NULL;

    gboolean has_valid_parent_directory = FALSE;
    gboolean has_valid_name = FALSE;

    if (context == NULL ||
        context->create_button == NULL ||
        context->investigation_name_entry == NULL)
    {
        return;
    }

    has_valid_parent_directory =
        context->parent_directory != NULL &&
        context->parent_directory[0] != '\0' &&
        g_file_test(
            context->parent_directory,
            G_FILE_TEST_IS_DIR
        );

    investigation_name = gtk_editable_get_text(
        GTK_EDITABLE(
            context->investigation_name_entry
        )
    );

    has_valid_name =
        create_investigation_dialog_normalize_name(
            investigation_name,
            NULL
        );

    gtk_widget_set_sensitive(
        context->create_button,
        has_valid_parent_directory && has_valid_name
    );
}

/**
 * @brief Termine le dialogue et appelle son callback une seule fois.
 *
 * @param context Contexte du dialogue.
 * @param parent_directory Dossier parent ou NULL en cas d'annulation.
 * @param investigation_name Nom nettoyé ou NULL en cas d'annulation.
 */
static void create_investigation_dialog_complete(
    CreateInvestigationDialogContext *context,
    const char *parent_directory,
    const char *investigation_name
)
{
    if (context == NULL ||
        context->completed)
    {
        return;
    }

    context->completed = TRUE;

    if (context->callback != NULL)
    {
        context->callback(
            parent_directory,
            investigation_name,
            context->user_data
        );
    }

    if (context->window != NULL)
    {
        gtk_window_destroy(
            context->window
        );
    }
}

/**
 * @brief Traite les modifications du nom de l'enquête.
 *
 * @param editable Champ ayant été modifié.
 * @param user_data Contexte du dialogue.
 */
static void create_investigation_dialog_on_name_changed(
    GtkEditable *editable,
    gpointer user_data
)
{
    CreateInvestigationDialogContext *context = user_data;

    (void) editable;

    create_investigation_dialog_update_create_button(
        context
    );
}

/**
 * @brief Traite le résultat du sélecteur de dossier parent.
 *
 * La référence passée dans user_data maintient la fenêtre en vie pendant
 * toute l'opération asynchrone.
 *
 * @param source_object GtkFileDialog ayant lancé l'opération.
 * @param result Résultat asynchrone.
 * @param user_data Référence vers la fenêtre du dialogue.
 */
static void create_investigation_dialog_on_parent_selected(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data
)
{
    GtkFileDialog *file_dialog = NULL;
    GtkWindow *dialog_window = NULL;

    CreateInvestigationDialogContext *context = NULL;

    GFile *selected_folder = NULL;
    GError *error = NULL;

    char *selected_path = NULL;

    file_dialog = GTK_FILE_DIALOG(
        source_object
    );

    dialog_window = GTK_WINDOW(
        user_data
    );

    context = g_object_get_data(
        G_OBJECT(dialog_window),
        "create-investigation-dialog-context"
    );

    selected_folder = gtk_file_dialog_select_folder_finish(
        file_dialog,
        result,
        &error
    );

    /*
     * Le dialogue de création a pu être fermé pendant que le sélecteur
     * asynchrone était encore actif.
     */
    if (context == NULL ||
        context->completed)
    {
        g_clear_error(&error);

        if (selected_folder != NULL)
        {
            g_object_unref(selected_folder);
        }

        g_object_unref(dialog_window);
        return;
    }

    if (selected_folder == NULL)
    {
        if (error != NULL &&
            !g_error_matches(
                error,
                GTK_DIALOG_ERROR,
                GTK_DIALOG_ERROR_DISMISSED
            ))
        {
            g_warning(
                "Impossible de sélectionner le dossier parent : %s",
                error->message
            );
        }

        g_clear_error(&error);
        g_object_unref(dialog_window);

        return;
    }

    selected_path = g_file_get_path(
        selected_folder
    );

    if (selected_path == NULL)
    {
        g_warning(
            "Le dossier sélectionné ne possède pas de chemin local."
        );

        g_object_unref(selected_folder);
        g_object_unref(dialog_window);

        return;
    }

    if (!g_file_test(
            selected_path,
            G_FILE_TEST_IS_DIR
        ))
    {
        g_warning(
            "Le chemin sélectionné n'est pas un dossier existant : '%s'.",
            selected_path
        );

        g_free(selected_path);
        g_object_unref(selected_folder);
        g_object_unref(dialog_window);

        return;
    }

    g_free(context->parent_directory);

    context->parent_directory = selected_path;
    selected_path = NULL;

    gtk_editable_set_text(
        GTK_EDITABLE(
            context->parent_directory_entry
        ),
        context->parent_directory
    );

    create_investigation_dialog_update_create_button(
        context
    );

    g_object_unref(selected_folder);
    g_object_unref(dialog_window);
}

/**
 * @brief Ouvre le sélecteur du dossier parent.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Contexte du dialogue.
 */
static void create_investigation_dialog_on_browse_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    CreateInvestigationDialogContext *context = user_data;
    GtkFileDialog *file_dialog = NULL;

    (void) button;

    if (context == NULL ||
        context->completed ||
        context->window == NULL)
    {
        return;
    }

    file_dialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(
        file_dialog,
        "Sélectionner le dossier parent"
    );

    gtk_file_dialog_set_modal(
        file_dialog,
        TRUE
    );

    gtk_file_dialog_select_folder(
        file_dialog,
        context->window,
        NULL,
        create_investigation_dialog_on_parent_selected,
        g_object_ref(context->window)
    );

    g_object_unref(file_dialog);
}

/**
 * @brief Annule la création.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Contexte du dialogue.
 */
static void create_investigation_dialog_on_cancel_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    CreateInvestigationDialogContext *context = user_data;

    (void) button;

    create_investigation_dialog_complete(
        context,
        NULL,
        NULL
    );
}

/**
 * @brief Valide les informations et termine le dialogue.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Contexte du dialogue.
 */
static void create_investigation_dialog_on_create_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    CreateInvestigationDialogContext *context = user_data;

    const char *entry_text = NULL;
    char *trimmed_name = NULL;

    (void) button;

    if (context == NULL ||
        context->completed)
    {
        return;
    }

    if (context->parent_directory == NULL ||
        !g_file_test(
            context->parent_directory,
            G_FILE_TEST_IS_DIR
        ))
    {
        create_investigation_dialog_update_create_button(
            context
        );

        return;
    }

    entry_text = gtk_editable_get_text(
        GTK_EDITABLE(
            context->investigation_name_entry
        )
    );

    if (!create_investigation_dialog_normalize_name(
            entry_text,
            &trimmed_name
        ))
    {
        create_investigation_dialog_update_create_button(
            context
        );

        return;
    }

    create_investigation_dialog_complete(
        context,
        context->parent_directory,
        trimmed_name
    );

    g_free(trimmed_name);
}

/**
 * @brief Intercepte la fermeture de la fenêtre.
 *
 * Une fermeture avec le bouton système est traitée comme une annulation.
 *
 * @param window Fenêtre demandant sa fermeture.
 * @param user_data Contexte du dialogue.
 *
 * @return TRUE car la destruction est gérée par le module.
 */
static gboolean create_investigation_dialog_on_close_request(
    GtkWindow *window,
    gpointer user_data
)
{
    CreateInvestigationDialogContext *context = user_data;

    (void) window;

    create_investigation_dialog_complete(
        context,
        NULL,
        NULL
    );

    return TRUE;
}

void create_investigation_dialog_present(
    GtkWindow *parent_window,
    CreateInvestigationDialogCallback callback,
    gpointer user_data
)
{
    CreateInvestigationDialogContext *context = NULL;

    GtkWidget *main_box = NULL;

    GtkWidget *parent_label = NULL;
    GtkWidget *parent_row = NULL;
    GtkWidget *browse_button = NULL;

    GtkWidget *name_label = NULL;

    GtkWidget *action_box = NULL;
    GtkWidget *cancel_button = NULL;

    context = g_new0(
        CreateInvestigationDialogContext,
        1
    );

    context->callback = callback;
    context->user_data = user_data;

    context->window = GTK_WINDOW(
        gtk_window_new()
    );

    gtk_window_set_title(
        context->window,
        "Nouvelle enquête"
    );

    gtk_window_set_default_size(
        context->window,
        CREATE_INVESTIGATION_DIALOG_WIDTH,
        -1
    );

    gtk_window_set_modal(
        context->window,
        TRUE
    );

    gtk_window_set_resizable(
        context->window,
        FALSE
    );

    if (parent_window != NULL)
    {
        gtk_window_set_transient_for(
            context->window,
            parent_window
        );

        gtk_window_set_destroy_with_parent(
            context->window,
            TRUE
        );
    }

    /*
     * Le contexte est attaché à la fenêtre.
     *
     * Il sera libéré lorsque la dernière référence vers la fenêtre
     * disparaîtra, y compris si un sélecteur asynchrone est encore actif.
     */
    g_object_set_data_full(
        G_OBJECT(context->window),
        "create-investigation-dialog-context",
        context,
        (GDestroyNotify)
            create_investigation_dialog_context_free
    );

    main_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        CREATE_INVESTIGATION_DIALOG_SPACING
    );

    gtk_widget_set_margin_start(
        main_box,
        CREATE_INVESTIGATION_DIALOG_MARGIN
    );

    gtk_widget_set_margin_end(
        main_box,
        CREATE_INVESTIGATION_DIALOG_MARGIN
    );

    gtk_widget_set_margin_top(
        main_box,
        CREATE_INVESTIGATION_DIALOG_MARGIN
    );

    gtk_widget_set_margin_bottom(
        main_box,
        CREATE_INVESTIGATION_DIALOG_MARGIN
    );

    parent_label = gtk_label_new(
        "Dossier parent :"
    );

    gtk_widget_set_halign(
        parent_label,
        GTK_ALIGN_START
    );

    parent_row = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        8
    );

    context->parent_directory_entry = gtk_entry_new();

    gtk_editable_set_editable(
        GTK_EDITABLE(
            context->parent_directory_entry
        ),
        FALSE
    );

    gtk_widget_set_hexpand(
        context->parent_directory_entry,
        TRUE
    );

    gtk_entry_set_placeholder_text(
        GTK_ENTRY(
            context->parent_directory_entry
        ),
        "Aucun dossier sélectionné"
    );

    browse_button = gtk_button_new_with_label(
        "Parcourir"
    );

    gtk_box_append(
        GTK_BOX(parent_row),
        context->parent_directory_entry
    );

    gtk_box_append(
        GTK_BOX(parent_row),
        browse_button
    );

    name_label = gtk_label_new(
        "Nom de l'enquête :"
    );

    gtk_widget_set_halign(
        name_label,
        GTK_ALIGN_START
    );

    context->investigation_name_entry = gtk_entry_new();

    gtk_entry_set_placeholder_text(
        GTK_ENTRY(
            context->investigation_name_entry
        ),
        "Exemple : Arnaque_Billets"
    );

    action_box = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        8
    );

    gtk_widget_set_halign(
        action_box,
        GTK_ALIGN_END
    );

    cancel_button = gtk_button_new_with_label(
        "Annuler"
    );

    context->create_button = gtk_button_new_with_label(
        "Créer"
    );

    gtk_widget_add_css_class(
        context->create_button,
        "suggested-action"
    );

    gtk_widget_set_sensitive(
        context->create_button,
        FALSE
    );

    gtk_box_append(
        GTK_BOX(action_box),
        cancel_button
    );

    gtk_box_append(
        GTK_BOX(action_box),
        context->create_button
    );

    gtk_box_append(
        GTK_BOX(main_box),
        parent_label
    );

    gtk_box_append(
        GTK_BOX(main_box),
        parent_row
    );

    gtk_box_append(
        GTK_BOX(main_box),
        name_label
    );

    gtk_box_append(
        GTK_BOX(main_box),
        context->investigation_name_entry
    );

    gtk_box_append(
        GTK_BOX(main_box),
        action_box
    );

    gtk_window_set_child(
        context->window,
        main_box
    );

    g_signal_connect(
        context->window,
        "close-request",
        G_CALLBACK(
            create_investigation_dialog_on_close_request
        ),
        context
    );

    g_signal_connect(
        browse_button,
        "clicked",
        G_CALLBACK(
            create_investigation_dialog_on_browse_clicked
        ),
        context
    );

    g_signal_connect(
        context->investigation_name_entry,
        "changed",
        G_CALLBACK(
            create_investigation_dialog_on_name_changed
        ),
        context
    );

    g_signal_connect(
        cancel_button,
        "clicked",
        G_CALLBACK(
            create_investigation_dialog_on_cancel_clicked
        ),
        context
    );

    g_signal_connect(
        context->create_button,
        "clicked",
        G_CALLBACK(
            create_investigation_dialog_on_create_clicked
        ),
        context
    );

    gtk_window_present(
        context->window
    );
}
