/******************************************************************************
 * @file task_panel.c
 * @brief Implémentation du panneau GTK affichant les tâches.
 ******************************************************************************/

#include "widgets/task_panel.h"

/**
 * @brief Intervalle de rafraîchissement du panneau, en millisecondes.
 */
#define TASK_PANEL_REFRESH_INTERVAL_MS 250

/**
 * @brief Texte affiché lorsqu’aucune tâche n’est suivie.
 */
#define TASK_PANEL_EMPTY_TEXT \
    "Aucune tâche en cours ou terminée"

/**
 * @struct TaskPanelRow
 * @brief Widgets persistants associés à une tâche.
 *
 * Les widgets sont créés une seule fois. Le rafraîchissement périodique
 * modifie seulement leur contenu.
 */
typedef struct
{
    BackgroundTask *task;

    GtkWidget *frame;
    GtkWidget *state_label;
    GtkWidget *progress_bar;
    GtkWidget *detail_label;
    GtkWidget *cancel_button;
    GtkWidget *remove_button;
} TaskPanelRow;

/**
 * @struct TaskPanel
 * @brief État interne du panneau d’activité.
 *
 * TaskManager est emprunté et doit rester valide pendant toute la durée
 * de vie du panneau.
 */
struct TaskPanel
{
    TaskManager *task_manager;

    GtkWidget *root_box;
    GtkWidget *header_box;
    GtkWidget *title_label;
    GtkWidget *clear_button;

    GtkWidget *scrolled_window;
    GtkWidget *task_list_box;

    GPtrArray *rows;

    guint refresh_source_id;
};

/**
 * @brief Retourne le libellé correspondant à l’état d’une tâche.
 *
 * @param state État de la tâche.
 *
 * @return Libellé statique.
 */
static const char *task_panel_get_state_label(
    BackgroundTaskState state
)
{
    switch (state)
    {
        case BACKGROUND_TASK_STATE_PENDING:
            return "En attente";

        case BACKGROUND_TASK_STATE_RUNNING:
            return "En cours";

        case BACKGROUND_TASK_STATE_COMPLETED:
            return "Terminée";

        case BACKGROUND_TASK_STATE_FAILED:
            return "Échouée";

        case BACKGROUND_TASK_STATE_CANCELLED:
            return "Annulée";

        default:
            return "État inconnu";
    }
}

/**
 * @brief Supprime tous les widgets enfants d’une boîte GTK.
 *
 * @param box Boîte à vider.
 */
static void task_panel_clear_box(
    GtkWidget *box
)
{
    GtkWidget *child = NULL;

    if (box == NULL)
    {
        return;
    }

    child = gtk_widget_get_first_child(
        box
    );

    while (child != NULL)
    {
        gtk_box_remove(
            GTK_BOX(box),
            child
        );

        child = gtk_widget_get_first_child(
            box
        );
    }
}

/**
 * @brief Demande l’annulation d’une tâche.
 *
 * La tâche est conservée dans les données du bouton avec une référence
 * indépendante.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Pointeur vers TaskPanel.
 */
static void task_panel_on_cancel_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    TaskPanel *task_panel = user_data;
    BackgroundTask *task = NULL;

    if (button == NULL ||
        task_panel == NULL)
    {
        return;
    }

    task = g_object_get_data(
        G_OBJECT(button),
        "labfy-background-task"
    );

    if (task == NULL)
    {
        return;
    }

    background_task_cancel(
        task
    );
}

/**
 * @brief Retire les tâches terminées du gestionnaire.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Pointeur vers TaskPanel.
 */
static void task_panel_on_clear_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    TaskPanel *task_panel = user_data;

    (void) button;

    if (task_panel == NULL ||
        task_panel->task_manager == NULL)
    {
        return;
    }

    task_manager_remove_finished(
        task_panel->task_manager
    );
}

/** @brief Retire uniquement la tâche terminée de cette ligne. */
static void task_panel_on_remove_clicked(GtkButton *button, gpointer user_data)
{
    TaskPanel *task_panel = user_data;
    BackgroundTask *task = NULL;
    if (task_panel == NULL || task_panel->task_manager == NULL) return;
    task = g_object_get_data(G_OBJECT(button), "labfy-background-task");
    if (task != NULL && background_task_get_state(task) !=
        BACKGROUND_TASK_STATE_RUNNING && background_task_get_state(task) !=
        BACKGROUND_TASK_STATE_PENDING)
        task_manager_remove(task_panel->task_manager, task);
}

/**
 * @brief Réagit aux changements structurels du gestionnaire.
 *
 * @param task_manager Gestionnaire ayant changé.
 * @param user_data Pointeur vers TaskPanel.
 */
static void task_panel_on_manager_changed(
    TaskManager *task_manager,
    gpointer user_data
)
{
    TaskPanel *task_panel = user_data;

    if (task_panel == NULL ||
        task_manager == NULL ||
        task_panel->task_manager != task_manager)
    {
        return;
    }

    task_panel_refresh(
        task_panel
    );
}

/**
 * @brief Met à jour les lignes existantes sans reconstruire la liste.
 *
 * @param task_panel Panneau à mettre à jour.
 */
static void task_panel_update_rows(
    TaskPanel *task_panel
);

/**
 * @brief Rafraîchit périodiquement les progressions.
 *
 * @param user_data Pointeur vers TaskPanel.
 *
 * @return G_SOURCE_CONTINUE.
 */
static gboolean task_panel_on_refresh_timeout(
    gpointer user_data
)
{
    TaskPanel *task_panel = user_data;

    if (task_panel == NULL)
    {
        return G_SOURCE_REMOVE;
    }

    task_panel_update_rows(
        task_panel
    );

    return G_SOURCE_CONTINUE;
}

/**
 * @brief Crée le texte secondaire d’une tâche.
 *
 * @param task Tâche concernée.
 * @param state État courant.
 * @param progress Progression courante.
 * @param status_message Message de progression facultatif.
 * @param error Erreur finale facultative.
 *
 * @return Nouvelle chaîne à libérer avec g_free().
 */
static char *task_panel_create_detail_text(
    const BackgroundTask *task,
    BackgroundTaskState state,
    double progress,
    const char *status_message,
    const GError *error
)
{
    const char *state_label = NULL;

    if (task == NULL)
    {
        return g_strdup(
            "Tâche invalide"
        );
    }

    state_label = task_panel_get_state_label(
        state
    );

    if (state == BACKGROUND_TASK_STATE_RUNNING)
    {
        if (status_message != NULL &&
            status_message[0] != '\0')
        {
            return g_strdup_printf(
                "%s — %.0f %% — %s",
                state_label,
                progress * 100.0,
                status_message
            );
        }

        return g_strdup_printf(
            "%s — %.0f %%",
            state_label,
            progress * 100.0
        );
    }

    if (state == BACKGROUND_TASK_STATE_FAILED &&
        error != NULL)
    {
        return g_strdup_printf(
            "%s — %s",
            state_label,
            error->message
        );
    }

    if (status_message != NULL &&
        status_message[0] != '\0')
    {
        return g_strdup_printf(
            "%s — %s",
            state_label,
            status_message
        );
    }

    return g_strdup(
        state_label
    );
}

/**
 * @brief Libère les données privées d'une ligne.
 *
 * @param user_data Pointeur vers TaskPanelRow.
 */
static void task_panel_row_free(
    gpointer user_data
)
{
    TaskPanelRow *task_row = user_data;

    if (task_row == NULL)
    {
        return;
    }

    background_task_unref(
        task_row->task
    );

    task_row->task = NULL;

    g_free(
        task_row
    );
}

/**
 * @brief Met à jour l'affichage d'une ligne existante.
 *
 * Aucun widget n'est détruit ici.
 *
 * @param task_row Ligne à mettre à jour.
 */
static void task_panel_row_update(
    TaskPanelRow *task_row
)
{
    BackgroundTaskState state;
    double progress = 0.0;

    char *status_message = NULL;
    char *detail_text = NULL;

    GError *error = NULL;

    if (task_row == NULL ||
        task_row->task == NULL)
    {
        return;
    }

    state = background_task_get_state(
        task_row->task
    );

    progress = background_task_get_progress(
        task_row->task
    );

    status_message =
        background_task_dup_status_message(
            task_row->task
        );

    error = background_task_dup_error(
        task_row->task
    );

    detail_text = task_panel_create_detail_text(
        task_row->task,
        state,
        progress,
        status_message,
        error
    );

    gtk_label_set_text(
        GTK_LABEL(task_row->state_label),
        task_panel_get_state_label(
            state
        )
    );

    gtk_progress_bar_set_fraction(
        GTK_PROGRESS_BAR(
            task_row->progress_bar
        ),
        state == BACKGROUND_TASK_STATE_COMPLETED
            ? 1.0
            : progress
    );

    gtk_label_set_text(
        GTK_LABEL(task_row->detail_label),
        detail_text != NULL
            ? detail_text
            : ""
    );

    /*
     * Le bouton existe en permanence, mais n'est visible que lorsque
     * la tâche peut être annulée.
     */
    gtk_widget_set_visible(
        task_row->cancel_button,
        state == BACKGROUND_TASK_STATE_RUNNING
    );

    gtk_widget_set_sensitive(
        task_row->cancel_button,
        state == BACKGROUND_TASK_STATE_RUNNING
    );
    gtk_widget_set_visible(task_row->remove_button,
        state != BACKGROUND_TASK_STATE_RUNNING &&
        state != BACKGROUND_TASK_STATE_PENDING);

    g_clear_error(
        &error
    );

    g_free(
        detail_text
    );

    g_free(
        status_message
    );
}

/**
 * @brief Crée une ligne GTK persistante pour une tâche.
 *
 * @param task_panel Panneau propriétaire.
 * @param task Tâche à afficher.
 *
 * @return Nouvelle ligne, ou NULL.
 */
static TaskPanelRow *task_panel_create_task_row(
    TaskPanel *task_panel,
    BackgroundTask *task
)
{
    TaskPanelRow *task_row = NULL;

    GtkWidget *row_box = NULL;
    GtkWidget *header_box = NULL;
    GtkWidget *title_label = NULL;

    const char *title = NULL;

    if (task_panel == NULL ||
        task == NULL)
    {
        return NULL;
    }

    task_row = g_new0(
        TaskPanelRow,
        1
    );

    task_row->task = background_task_ref(
        task
    );

    task_row->frame = gtk_frame_new(
        NULL
    );

    row_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        6
    );

    gtk_widget_set_margin_start(
        row_box,
        10
    );

    gtk_widget_set_margin_end(
        row_box,
        10
    );

    gtk_widget_set_margin_top(
        row_box,
        8
    );

    gtk_widget_set_margin_bottom(
        row_box,
        8
    );

    header_box = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        8
    );

    title = background_task_get_title(
        task
    );

    title_label = gtk_label_new(
        title != NULL
            ? title
            : "Tâche sans titre"
    );

    gtk_widget_set_hexpand(
        title_label,
        TRUE
    );

    gtk_widget_set_halign(
        title_label,
        GTK_ALIGN_START
    );

    gtk_label_set_xalign(
        GTK_LABEL(title_label),
        0.0F
    );

    gtk_label_set_ellipsize(
        GTK_LABEL(title_label),
        PANGO_ELLIPSIZE_END
    );

    task_row->state_label = gtk_label_new(
        NULL
    );

    gtk_widget_set_halign(
        task_row->state_label,
        GTK_ALIGN_END
    );

    task_row->cancel_button =
        gtk_button_new_with_label(
            "Annuler"
        );
    task_row->remove_button = gtk_button_new_with_label("Nettoyer");

    gtk_widget_set_halign(
        task_row->cancel_button,
        GTK_ALIGN_END
    );

    gtk_widget_set_valign(
        task_row->cancel_button,
        GTK_ALIGN_CENTER
    );

    /*
     * TaskPanelRow conserve déjà une référence sur la tâche.
     * Le bouton emprunte donc simplement ce pointeur.
     */
    g_object_set_data(
        G_OBJECT(task_row->cancel_button),
        "labfy-background-task",
        task_row->task
    );

    g_signal_connect(
        task_row->cancel_button,
        "clicked",
        G_CALLBACK(
            task_panel_on_cancel_clicked
        ),
        task_panel
    );
    g_object_set_data(G_OBJECT(task_row->remove_button),
        "labfy-background-task", task_row->task);
    g_signal_connect(task_row->remove_button, "clicked",
        G_CALLBACK(task_panel_on_remove_clicked), task_panel);

    gtk_box_append(
        GTK_BOX(header_box),
        title_label
    );

    gtk_box_append(
        GTK_BOX(header_box),
        task_row->state_label
    );

    gtk_box_append(
        GTK_BOX(header_box),
        task_row->cancel_button
    );
    gtk_box_append(GTK_BOX(header_box), task_row->remove_button);

    task_row->progress_bar =
        gtk_progress_bar_new();

    gtk_progress_bar_set_show_text(
        GTK_PROGRESS_BAR(
            task_row->progress_bar
        ),
        TRUE
    );

    task_row->detail_label = gtk_label_new(
        NULL
    );

    gtk_widget_set_halign(
        task_row->detail_label,
        GTK_ALIGN_START
    );

    gtk_label_set_xalign(
        GTK_LABEL(task_row->detail_label),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(task_row->detail_label),
        TRUE
    );

    gtk_label_set_wrap_mode(
        GTK_LABEL(task_row->detail_label),
        PANGO_WRAP_WORD_CHAR
    );

    gtk_label_set_selectable(
        GTK_LABEL(task_row->detail_label),
        TRUE
    );

    gtk_box_append(
        GTK_BOX(row_box),
        header_box
    );

    gtk_box_append(
        GTK_BOX(row_box),
        task_row->progress_bar
    );

    gtk_box_append(
        GTK_BOX(row_box),
        task_row->detail_label
    );

    gtk_frame_set_child(
        GTK_FRAME(task_row->frame),
        row_box
    );

    task_panel_row_update(
        task_row
    );

    return task_row;
}

/**
 * @brief Supprime les widgets et les données de toutes les lignes.
 *
 * @param task_panel Panneau à vider.
 */
static void task_panel_clear_rows(
    TaskPanel *task_panel
)
{
    if (task_panel == NULL)
    {
        return;
    }

    task_panel_clear_box(
        task_panel->task_list_box
    );

    if (task_panel->rows != NULL)
    {
        g_ptr_array_set_size(
            task_panel->rows,
            0
        );
    }
}

/**
 * @brief Met à jour toutes les lignes persistantes.
 *
 * @param task_panel Panneau concerné.
 */
static void task_panel_update_rows(
    TaskPanel *task_panel
)
{
    guint index = 0;

    if (task_panel == NULL ||
        task_panel->rows == NULL)
    {
        return;
    }

    for (index = 0;
         index < task_panel->rows->len;
         index++)
    {
        TaskPanelRow *task_row = NULL;

        task_row = g_ptr_array_index(
            task_panel->rows,
            index
        );

        task_panel_row_update(
            task_row
        );
    }
}

TaskPanel *task_panel_new(
    TaskManager *task_manager
)
{
    TaskPanel *task_panel = NULL;

    if (task_manager == NULL)
    {
        return NULL;
    }

    task_panel = g_new0(
        TaskPanel,
        1
    );

    task_panel->task_manager =
        task_manager;

    task_panel->rows =
        g_ptr_array_new_with_free_func(
            task_panel_row_free
        );

    task_panel->root_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        8
    );

    task_panel->header_box = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        8
    );

    gtk_widget_set_margin_start(
        task_panel->header_box,
        8
    );

    gtk_widget_set_margin_end(
        task_panel->header_box,
        8
    );

    gtk_widget_set_margin_top(
        task_panel->header_box,
        8
    );

    task_panel->title_label = gtk_label_new(
        "Activité"
    );

    gtk_widget_set_hexpand(
        task_panel->title_label,
        TRUE
    );

    gtk_widget_set_halign(
        task_panel->title_label,
        GTK_ALIGN_START
    );

    gtk_label_set_xalign(
        GTK_LABEL(task_panel->title_label),
        0.0F
    );

    task_panel->clear_button =
        gtk_button_new_with_label(
            "Nettoyer"
        );

    g_signal_connect(
        task_panel->clear_button,
        "clicked",
        G_CALLBACK(
            task_panel_on_clear_clicked
        ),
        task_panel
    );

    gtk_box_append(
        GTK_BOX(task_panel->header_box),
        task_panel->title_label
    );

    gtk_box_append(
        GTK_BOX(task_panel->header_box),
        task_panel->clear_button
    );

    task_panel->task_list_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        8
    );

    gtk_widget_set_margin_start(
        task_panel->task_list_box,
        8
    );

    gtk_widget_set_margin_end(
        task_panel->task_list_box,
        8
    );

    gtk_widget_set_margin_bottom(
        task_panel->task_list_box,
        8
    );

    task_panel->scrolled_window =
        gtk_scrolled_window_new();

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(
            task_panel->scrolled_window
        ),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(
            task_panel->scrolled_window
        ),
        task_panel->task_list_box
    );

    gtk_widget_set_vexpand(
        task_panel->scrolled_window,
        TRUE
    );

    gtk_box_append(
        GTK_BOX(task_panel->root_box),
        task_panel->header_box
    );

    gtk_box_append(
        GTK_BOX(task_panel->root_box),
        task_panel->scrolled_window
    );

    task_manager_set_changed_callback(
        task_manager,
        task_panel_on_manager_changed,
        task_panel,
        NULL
    );

    task_panel->refresh_source_id =
        g_timeout_add(
            TASK_PANEL_REFRESH_INTERVAL_MS,
            task_panel_on_refresh_timeout,
            task_panel
        );

    if (task_panel->refresh_source_id == 0)
    {
        task_manager_set_changed_callback(
            task_manager,
            NULL,
            NULL,
            NULL
        );

        g_ptr_array_unref(
            task_panel->rows
        );

        task_panel->rows = NULL;

        g_free(
            task_panel
        );

        return NULL;
    }

    task_panel_refresh(
        task_panel
    );

    return task_panel;
}

GtkWidget *task_panel_get_widget(
    const TaskPanel *task_panel
)
{
    if (task_panel == NULL)
    {
        return NULL;
    }

    return task_panel->root_box;
}

void task_panel_refresh(
    TaskPanel *task_panel
)
{
    gsize task_count = 0;
    gsize index = 0;

    if (task_panel == NULL ||
        task_panel->task_manager == NULL ||
        task_panel->task_list_box == NULL ||
        task_panel->rows == NULL)
    {
        return;
    }

    /*
     * La liste n'est reconstruite que lorsque TaskManager signale
     * un ajout ou une suppression.
     */
    task_panel_clear_rows(
        task_panel
    );

    task_count = task_manager_get_count(
        task_panel->task_manager
    );

    gtk_widget_set_sensitive(
        task_panel->clear_button,
        task_count > 0
    );

    if (task_count == 0)
    {
        GtkWidget *empty_label = NULL;

        empty_label = gtk_label_new(
            TASK_PANEL_EMPTY_TEXT
        );

        gtk_widget_set_margin_top(
            empty_label,
            16
        );

        gtk_widget_set_margin_bottom(
            empty_label,
            16
        );

        gtk_widget_set_halign(
            empty_label,
            GTK_ALIGN_CENTER
        );

        gtk_box_append(
            GTK_BOX(task_panel->task_list_box),
            empty_label
        );

        return;
    }

    for (index = 0;
         index < task_count;
         index++)
    {
        BackgroundTask *task = NULL;
        TaskPanelRow *task_row = NULL;

        task = task_manager_get_task(
            task_panel->task_manager,
            index
        );

        if (task == NULL)
        {
            continue;
        }

        task_row = task_panel_create_task_row(
            task_panel,
            task
        );

        if (task_row != NULL)
        {
            gtk_box_append(
                GTK_BOX(
                    task_panel->task_list_box
                ),
                task_row->frame
            );

            g_ptr_array_add(
                task_panel->rows,
                task_row
            );
        }

        background_task_unref(
            task
        );
    }
}

void task_panel_free(
    TaskPanel *task_panel
)
{
    if (task_panel == NULL)
    {
        return;
    }

    /*
     * Le callback du manager ne doit plus pouvoir utiliser TaskPanel.
     */
    if (task_panel->task_manager != NULL)
    {
        task_manager_set_changed_callback(
            task_panel->task_manager,
            NULL,
            NULL,
            NULL
        );
    }

    /*
     * Arrêt du rafraîchissement périodique.
     */
    if (task_panel->refresh_source_id != 0)
    {
        g_source_remove(
            task_panel->refresh_source_id
        );

        task_panel->refresh_source_id = 0;
    }

    if (task_panel->clear_button != NULL)
    {
        g_signal_handlers_disconnect_by_data(
            task_panel->clear_button,
            task_panel
        );
    }

    task_panel_clear_rows(
        task_panel
    );

    if (task_panel->rows != NULL)
    {
        g_ptr_array_unref(
            task_panel->rows
        );

        task_panel->rows = NULL;
    }

    task_panel->task_manager = NULL;

    g_free(
        task_panel
    );
}
