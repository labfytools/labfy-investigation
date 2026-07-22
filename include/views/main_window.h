/******************************************************************************
 * @file main_window.h
 * @brief Interface publique de la fenêtre principale.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_MAIN_WINDOW_H
#define LABFY_INVESTIGATION_MAIN_WINDOW_H

#include "core/investigation_tree_model.h"
#include "widgets/investigation_tree_view.h"
#include "core/investigation_node.h"
#include "core/task_manager.h"
#include "widgets/evidence_category_model.h"
#include "models/evidence_record.h"
#include "models/osint_action_catalog.h"

#include <gtk/gtk.h>

/**
 * @brief Représentation opaque du graphe d'enquête.
 */
typedef struct InvestigationGraphModel InvestigationGraphModel;

/**
 * @brief Représentation opaque de la disposition du graphe.
 */
typedef struct InvestigationGraphLayout InvestigationGraphLayout;

/**
 * @brief Représentation opaque de la fenêtre principale.
 */
typedef struct MainWindow MainWindow;

/**
 * @brief Callback appelé lorsque l'utilisateur demande une nouvelle enquête.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowNewInvestigationCallback)(
    gpointer user_data
);

/**
 * @brief Callback appelé lorsque l'utilisateur demande
 *        l'import d'une preuve.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowImportEvidenceCallback)(
    gpointer user_data
);

/** @brief Callback appelé pour ajouter un compte de réseau social. */
typedef void (*MainWindowAddSocialAccountCallback)(
    gpointer user_data
);
/** @brief Callback appelé pour ajouter une personne. */
typedef void (*MainWindowAddPersonCallback)(gpointer user_data);

/**
 * @brief Callback appelé lorsque l'utilisateur revient au graphe.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowShowGraphCallback)(
    gpointer user_data
);

/**
 * @brief Callback appelé lorsqu'une preuve est sélectionnée.
 *
 * L'identifiant est emprunté et uniquement valide pendant l'appel.
 * Une sélection vide ou une catégorie transmet NULL.
 *
 * @param evidence_identifier UUID de la preuve, ou NULL.
 * @param user_data Données privées fournies par l'appelant.
 */
typedef void (*MainWindowEvidenceSelectionCallback)(
    const char *evidence_identifier,
    gpointer user_data
);

/**
 * @brief Callback appelé lorsque l'utilisateur demande une vérification.
 *
 * L'identifiant est emprunté et uniquement valide pendant l'appel.
 *
 * @param evidence_identifier UUID de la preuve.
 * @param user_data Données privées du callback.
 */
typedef void (*MainWindowVerifyEvidenceCallback)(
    const char *evidence_identifier,
    gpointer user_data
);

/** @brief Callback appelé pour modifier les métadonnées d'une preuve. */
typedef void (*MainWindowEditEvidenceCallback)(
    const char *evidence_identifier,
    gpointer user_data
);

/**
 * @brief Callback appelé après le déplacement effectif d'un nœud.
 *
 * @param entity_identifier UUID de l'entité déplacée.
 * @param x Coordonnée horizontale logique.
 * @param y Coordonnée verticale logique.
 * @param user_data Données privées du callback.
 */
typedef void (*MainWindowGraphNodeMovedCallback)(
    const char *entity_identifier,
    double x,
    double y,
    gpointer user_data
);

/**
 * @brief Callback appelé lors d'une demande de réinitialisation du graphe.
 *
 * @param user_data Données privées du callback.
 */
typedef void (*MainWindowResetGraphLayoutCallback)(
    gpointer user_data
);

/**
 * @brief Callback appelé lors d'une demande d'ajout d'une relation.
 *
 * @param source_entity_identifier UUID de l'entité source.
 * @param user_data Données privées du callback.
 */
typedef void (*MainWindowAddRelationCallback)(
    const char *source_entity_identifier,
    gpointer user_data
);

/**
 * @brief Callback appelé lors du déclenchement d'une action OSINT.
 *
 * @param action_identifier Identifiant stable de l'action.
 * @param target_identifier UUID de la sélection ciblée.
 * @param target_value Valeur métier ciblée.
 * @param user_data Données privées fournies par l'appelant.
 */
typedef void (*MainWindowOsintActionCallback)(
    const char *action_identifier,
    const char *target_identifier,
    const char *target_value,
    gpointer user_data
);

/**
 * @brief Définit le callback de vérification d'une preuve.
 *
 * MainWindow relaie la demande provenant du Workspace.
 *
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_verify_evidence_callback(
    MainWindow *main_window,
    MainWindowVerifyEvidenceCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de modification de la preuve sélectionnée.
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_edit_evidence_callback(
    MainWindow *main_window,
    MainWindowEditEvidenceCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de fin de déplacement d'un nœud.
 *
 * MainWindow relaie uniquement l'événement provenant du Workspace.
 *
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_graph_node_moved_callback(
    MainWindow *main_window,
    MainWindowGraphNodeMovedCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de demande de réinitialisation du graphe.
 *
 * MainWindow relaie uniquement la demande provenant du Workspace.
 *
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_reset_graph_layout_callback(
    MainWindow *main_window,
    MainWindowResetGraphLayoutCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de demande d'ajout d'une relation.
 *
 * MainWindow relaie uniquement la demande provenant du Workspace.
 *
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_add_relation_callback(
    MainWindow *main_window,
    MainWindowAddRelationCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de déclenchement des actions OSINT.
 *
 * MainWindow relaie uniquement la demande provenant du Workspace.
 *
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_osint_action_callback(
    MainWindow *main_window,
    MainWindowOsintActionCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback du bouton « Nouvelle enquête ».
 *
 * La fenêtre ne crée pas directement l'enquête. Elle transmet uniquement
 * la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic.
 * @param user_data Données transmises au callback.
 */
void main_window_set_new_investigation_callback(
    MainWindow *main_window,
    MainWindowNewInvestigationCallback callback,
    gpointer user_data
);

/**
 * @brief Crée une nouvelle fenêtre principale.
 *
 * MainWindow ne devient pas propriétaire de task_manager.
 *
 * @param application Application GTK.
 * @param task_manager Gestionnaire de tâches de l'application.
 *
 * @return Nouvelle fenêtre, ou NULL en cas d'échec.
 */
MainWindow *main_window_new(
    GtkApplication *application,
    TaskManager *task_manager
);

/**
 * @brief Affiche la fenêtre principale.
 *
 * @param main_window Fenêtre à afficher.
 */
void main_window_present(MainWindow *main_window);

/**
 * @brief Retourne la fenêtre GTK associée.
 *
 * @param main_window Fenêtre principale.
 *
 * @return Fenêtre GTK.
 */
GtkWindow *main_window_get_window(
    const MainWindow *main_window
);

/**
 * @brief Transmet un modèle d'arborescence à la fenêtre principale.
 *
 * La fenêtre principale ne devient pas propriétaire du modèle.
 * Elle le transmet uniquement à la Sidebar.
 *
 * @param main_window Fenêtre principale à mettre à jour.
 * @param tree_model  Modèle d'arborescence en lecture seule.
 */
void main_window_set_tree_model(
    MainWindow *main_window,
    const InvestigationTreeModel *tree_model
);

/**
 * @brief Transmet le modèle des catégories de preuves à la Sidebar.
 *
 * MainWindow ne devient pas propriétaire du modèle.
 * Le modèle doit rester valide pendant son affichage.
 *
 * Passer NULL vide l'onglet Preuves.
 *
 * @param main_window Fenêtre principale.
 * @param evidence_category_model Modèle de catégories, ou NULL.
 */
void main_window_set_evidence_model(
    MainWindow *main_window,
    EvidenceCategoryModel *evidence_category_model
);

/**
 * @brief Met à jour l'identité de l'enquête affichée.
 *
 * La fonction met à jour :
 *
 * - le titre de la fenêtre ;
 * - le texte de la barre d'état.
 *
 * Les chaînes sont copiées par GTK et restent la propriété de l'appelant.
 *
 * Cette fonction accepte des chaînes NULL ou vides.
 *
 * @param main_window Fenêtre principale à mettre à jour.
 * @param investigation_name Nom persistant de l'enquête.
 * @param investigation_root_path Chemin racine de l'enquête.
 */
void main_window_set_investigation(
    MainWindow *main_window,
    const char *investigation_name,
    const char *investigation_root_path
);

void main_window_set_tree_selection_callback(
    MainWindow *main_window,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de sélection d'une preuve.
 *
 * MainWindow transmet uniquement le callback à la Sidebar.
 *
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_evidence_selection_callback(
    MainWindow *main_window,
    MainWindowEvidenceSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Affiche dans le Workspace les informations du nœud sélectionné.
 *
 * La fenêtre principale ne devient pas propriétaire du nœud.
 *
 * @param main_window Fenêtre principale à mettre à jour.
 * @param node        Nœud sélectionné en lecture seule, ou NULL.
 */
void main_window_set_selected_node(
    MainWindow *main_window,
    const InvestigationNode *node
);

/**
 * @brief Affiche la fiche de la preuve sélectionnée.
 *
 * MainWindow ne conserve pas le EvidenceRecord.
 *
 * @param main_window Fenêtre principale.
 * @param evidence_record Preuve sélectionnée, ou NULL.
 */
void main_window_set_selected_evidence(
    MainWindow *main_window,
    const EvidenceRecord *evidence_record
);


/**
 * @brief Affiche l'état de chargement du graphe.
 *
 * MainWindow transmet uniquement la demande au Workspace.
 *
 * @param main_window Fenêtre principale.
 */
void main_window_set_graph_loading(
    MainWindow *main_window
);

/**
 * @brief Affiche le graphe chargé et sa disposition dans le Workspace.
 *
 * MainWindow et Workspace empruntent graph_model et graph_layout.
 *
 * Passer NULL pour graph_layout conserve le placement automatique.
 *
 * @param main_window Fenêtre principale.
 * @param graph_model Graphe emprunté, ou NULL.
 * @param graph_layout Disposition empruntée, ou NULL.
 */
void main_window_set_graph(
    MainWindow *main_window,
    const InvestigationGraphModel *graph_model,
    const InvestigationGraphLayout *graph_layout
);

/**
 * @brief Affiche une erreur de chargement du graphe.
 *
 * @param main_window Fenêtre principale.
 * @param message Message d'erreur, ou NULL.
 */
void main_window_set_graph_error(
    MainWindow *main_window,
    const char *message
);

/**
 * @brief Détache le graphe du Workspace.
 *
 * Aucun graphe n'est libéré par MainWindow.
 *
 * @param main_window Fenêtre principale.
 */
void main_window_clear_graph(
    MainWindow *main_window
);

/**
 * @brief Reconstruit visuellement la disposition automatique du graphe.
 *
 * Cette fonction ne modifie pas SQLite.
 *
 * @param main_window Fenêtre principale.
 */
void main_window_reset_graph_layout(
    MainWindow *main_window
);

/**
 * @brief Met à jour le texte de la barre d'état.
 *
 * Si status_text vaut NULL, le texte par défaut est affiché.
 *
 * @param main_window Fenêtre principale.
 * @param status_text Nouveau texte de statut, ou NULL.
 */
void main_window_set_status(
    MainWindow *main_window,
    const char *status_text
);

/**
 * @brief Transmet au workspace l'état d'un outil OSINT externe.
 *
 * @param main_window Fenêtre principale.
 * @param tool_identifier Identifiant stable de l'outil.
 * @param state État traduit par l'application.
 * @param version Version détectée facultative.
 */
void main_window_set_osint_tool_state(
    MainWindow *main_window,
    const char *tool_identifier,
    OsintActionToolState state,
    const char *version
);

/**
 * @brief Callback appelé lorsque l'utilisateur demande l'ouverture
 *        d'une enquête existante.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowOpenInvestigationCallback)(
    gpointer user_data
);

/**
 * @brief Callback appelé lorsque l'utilisateur demande à quitter.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowQuitCallback)(
    gpointer user_data
);

/**
 * @brief Définit le callback du bouton « Ouvrir une enquête ».
 *
 * La fenêtre transmet uniquement la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic.
 * @param user_data Données transmises au callback.
 */
void main_window_set_open_investigation_callback(
    MainWindow *main_window,
    MainWindowOpenInvestigationCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback du bouton « Quitter ».
 *
 * MainWindow transmet uniquement la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic.
 * @param user_data Données transmises au callback.
 */
void main_window_set_quit_callback(
    MainWindow *main_window,
    MainWindowQuitCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de l'action « Importer une preuve ».
 *
 * MainWindow transmet uniquement la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic.
 * @param user_data Données transmises au callback.
 */
void main_window_set_import_evidence_callback(
    MainWindow *main_window,
    MainWindowImportEvidenceCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de l'action d'ajout d'un compte social.
 * @param main_window Fenêtre principale.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void main_window_set_add_social_account_callback(
    MainWindow *main_window,
    MainWindowAddSocialAccountCallback callback,
    gpointer user_data
);
/** @brief Définit le callback d'ajout d'une personne. */
void main_window_set_add_person_callback(MainWindow *main_window,
    MainWindowAddPersonCallback callback, gpointer user_data);

/**
 * @brief Définit le callback du bouton « Revenir au graphe ».
 *
 * MainWindow transmet uniquement la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic, ou NULL.
 * @param user_data Données transmises au callback.
 */
void main_window_set_show_graph_callback(
    MainWindow *main_window,
    MainWindowShowGraphCallback callback,
    gpointer user_data
);

/**
 * @brief Active ou désactive l'action d'import d'une preuve.
 *
 * @param main_window Fenêtre principale.
 * @param enabled TRUE pour autoriser l'import.
 */
void main_window_set_import_evidence_enabled(
    MainWindow *main_window,
    gboolean enabled
);

/**
 * @brief Active ou désactive l'ajout d'un compte social.
 * @param main_window Fenêtre principale.
 * @param enabled TRUE si une enquête est ouverte.
 */
void main_window_set_add_social_account_enabled(
    MainWindow *main_window,
    gboolean enabled
);
/** @brief Active ou désactive l'ajout d'une personne. */
void main_window_set_add_person_enabled(MainWindow *main_window, gboolean enabled);

/**
 * @brief Libère les ressources de la fenêtre.
 *
 * @param main_window Fenêtre à détruire.
 */
void main_window_free(MainWindow *main_window);

#endif
