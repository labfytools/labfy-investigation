/******************************************************************************
 * @file workspace.h
 * @brief Interface publique de la zone principale de travail.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_WORKSPACE_H
#define LABFY_INVESTIGATION_WORKSPACE_H

#include "core/investigation_node.h"
#include "models/evidence_record.h"
#include "models/entity_record.h"
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
 * @brief Représentation opaque de la zone de travail.
 */
typedef struct Workspace Workspace;

/**
 * @brief Callback appelé lors d'une demande de vérification d'une preuve.
 *
 * @param evidence_identifier UUID de la preuve affichée.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*WorkspaceVerifyEvidenceCallback)(
    const char *evidence_identifier,
    gpointer user_data
);

/**
 * @brief Callback appelé après le déplacement effectif d'un nœud.
 *
 * Les coordonnées sont exprimées dans l'espace logique du graphe.
 *
 * @param node_identifier UUID du nœud déplacé.
 * @param x Coordonnée horizontale logique.
 * @param y Coordonnée verticale logique.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*WorkspaceGraphNodeMovedCallback)(
    const char *node_identifier,
    double x,
    double y,
    gpointer user_data
);

/**
 * @brief Callback appelé lors d'une demande de réinitialisation du graphe.
 *
 * Le Workspace ne modifie ni SQLite ni le modèle de disposition. Il relaie
 * uniquement la demande vers la couche applicative.
 *
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*WorkspaceResetGraphLayoutCallback)(
    gpointer user_data
);

/**
 * @brief Callback appelé lors d'une demande d'ajout d'une relation.
 *
 * @param source_entity_identifier UUID de l'entité source.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*WorkspaceAddRelationCallback)(
    const char *source_entity_identifier,
    gpointer user_data
);

/** @brief Callback appelé pour modifier la relation sélectionnée. */
typedef void (*WorkspaceEditRelationCallback)(
    const char *relation_identifier,
    gpointer user_data
);

/** @brief Callback appelé quand une relation devient sélectionnée. */
typedef void (*WorkspaceRelationSelectedCallback)(
    const char *relation_identifier,
    gpointer user_data
);

/** @brief Callback appelé pour catégoriser une personne. */
typedef void (*WorkspacePersonRoleCallback)(const char *entity_identifier,
    PersonRole role, gpointer user_data);
/** @brief Callback appelé pour modifier la confiance d'une personne. */
typedef void (*WorkspacePersonConfidenceCallback)(
    const char *entity_identifier, gint confidence, gpointer user_data);

/**
 * @brief Callback appelé lors du déclenchement d'une action OSINT.
 *
 * Les chaînes sont empruntées et uniquement valides pendant l'appel.
 *
 * @param action_identifier Identifiant stable de l'action.
 * @param target_identifier UUID de la sélection ciblée.
 * @param target_value Valeur métier ciblée par l'action.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*WorkspaceOsintActionCallback)(
    const char *action_identifier,
    const char *target_identifier,
    const char *target_value,
    gpointer user_data
);

/** @brief Callback appelé pour modifier les métadonnées d'une preuve. */
typedef void (*WorkspaceEditEvidenceCallback)(
    const char *evidence_identifier,
    gpointer user_data
);
/** @brief Callback appelé pour analyser une preuve EML. */
typedef void (*WorkspaceAnalyzeEmlCallback)(const char *evidence_identifier,
    gpointer user_data);

/**
 * @brief Crée une nouvelle zone de travail.
 *
 * @return Une nouvelle zone de travail, ou NULL en cas d'échec.
 */
Workspace *workspace_new(void);

/**
 * @brief Retourne le widget GTK racine de la zone de travail.
 *
 * Le widget retourné appartient au module Workspace.
 *
 * @param workspace Zone de travail à consulter.
 *
 * @return Widget racine, ou NULL.
 */
GtkWidget *workspace_get_widget(
    const Workspace *workspace
);

/**
 * @brief Affiche les informations d'un nœud sélectionné.
 *
 * Le Workspace emprunte le nœud.
 * Passer NULL restaure l'état principal courant.
 *
 * @param workspace Zone de travail.
 * @param node Nœud emprunté, ou NULL.
 */
void workspace_set_selected_node(
    Workspace *workspace,
    const InvestigationNode *node
);

/**
 * @brief Affiche la fiche détaillée d'une preuve.
 *
 * Les textes sont copiés dans les widgets. Le Workspace ne conserve pas le
 * pointeur EvidenceRecord.
 *
 * Passer NULL restaure l'état principal courant.
 *
 * @param workspace Zone de travail.
 * @param evidence_record Preuve empruntée, ou NULL.
 */
void workspace_set_selected_evidence(
    Workspace *workspace,
    const EvidenceRecord *evidence_record
);

/**
 * @brief Affiche l'aperçu local d'un fichier image ou vidéo.
 *
 * file_path doit être un chemin absolu déjà validé par l'application.
 * Passer NULL libère l'aperçu courant.
 */
void workspace_set_evidence_preview(Workspace *workspace,
    const char *file_path, const char *display_name);

/**
 * @brief Sélectionne une entité dans le graphe affiché.
 *
 * La sélection actualise également le volet de détails et le contexte OSINT.
 *
 * @param workspace Zone de travail.
 * @param entity_identifier UUID de l'entité à sélectionner.
 *
 * @return TRUE lorsque l'entité est trouvée et sélectionnée.
 */
gboolean workspace_select_graph_entity(
    Workspace *workspace,
    const char *entity_identifier
);

/**
 * @brief Sélectionne une relation dans le graphe affiché.
 *
 * @param workspace Zone de travail.
 * @param relation_identifier UUID de la relation à sélectionner.
 *
 * @return TRUE lorsque la relation est trouvée et sélectionnée.
 */
gboolean workspace_select_graph_relation(
    Workspace *workspace,
    const char *relation_identifier
);

/**
 * @brief Actualise l'état d'un outil requis par les actions OSINT.
 *
 * @param workspace Zone de travail.
 * @param tool_identifier Identifiant stable de l'outil.
 * @param state État traduit par la couche applicative.
 * @param version Version détectée facultative.
 */
void workspace_set_osint_tool_state(
    Workspace *workspace,
    const char *tool_identifier,
    OsintActionToolState state,
    const char *version
);

/**
 * @brief Définit le callback de déclenchement des actions OSINT.
 *
 * Le Workspace ne lance aucun outil externe. Il relaie uniquement l'action
 * et la valeur du contexte courant vers la couche applicative.
 *
 * @param workspace Zone de travail.
 * @param callback Callback facultatif.
 * @param user_data Données empruntées transmises au callback.
 */
void workspace_set_osint_action_callback(
    Workspace *workspace,
    WorkspaceOsintActionCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de modification de la preuve sélectionnée.
 * @param workspace Zone de travail.
 * @param callback Callback facultatif.
 * @param user_data Données empruntées transmises au callback.
 */
void workspace_set_edit_evidence_callback(
    Workspace *workspace,
    WorkspaceEditEvidenceCallback callback,
    gpointer user_data
);
/** @brief Définit le callback d'analyse locale d'une preuve EML. */
void workspace_set_analyze_eml_callback(Workspace *workspace,
    WorkspaceAnalyzeEmlCallback callback, gpointer user_data);

/**
 * @brief Définit le callback de vérification de la preuve affichée.
 *
 * Le Workspace emprunte callback et user_data.
 *
 * @param workspace Zone de travail.
 * @param callback Callback à utiliser, ou NULL.
 * @param user_data Données empruntées du callback.
 */
void workspace_set_verify_evidence_callback(
    Workspace *workspace,
    WorkspaceVerifyEvidenceCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de fin de déplacement d'un nœud.
 *
 * Le Workspace relaie uniquement l'événement provenant de la vue graphique.
 *
 * @param workspace Zone de travail.
 * @param callback Callback facultatif.
 * @param user_data Données empruntées transmises au callback.
 */
void workspace_set_graph_node_moved_callback(
    Workspace *workspace,
    WorkspaceGraphNodeMovedCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de demande de réinitialisation du graphe.
 *
 * Le Workspace emprunte callback et user_data.
 *
 * @param workspace Zone de travail.
 * @param callback Callback facultatif.
 * @param user_data Données empruntées transmises au callback.
 */
void workspace_set_reset_graph_layout_callback(
    Workspace *workspace,
    WorkspaceResetGraphLayoutCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de demande d'ajout d'une relation.
 *
 * Le Workspace relaie uniquement l'événement du volet de détails.
 *
 * @param workspace Zone de travail.
 * @param callback Callback facultatif.
 * @param user_data Données empruntées transmises au callback.
 */
void workspace_set_add_relation_callback(
    Workspace *workspace,
    WorkspaceAddRelationCallback callback,
    gpointer user_data
);

/** @brief Définit le callback de modification d'une relation. */
void workspace_set_edit_relation_callback(
    Workspace *workspace,
    WorkspaceEditRelationCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de sélection d'une relation.
 *
 * @param workspace Zone de travail.
 * @param callback Callback facultatif.
 * @param user_data Données empruntées transmises au callback.
 */
void workspace_set_relation_selected_callback(
    Workspace *workspace,
    WorkspaceRelationSelectedCallback callback,
    gpointer user_data
);

/**
 * @brief Affiche les preuves associées à la relation sélectionnée.
 *
 * @param workspace Zone de travail.
 * @param evidence_records Tableau emprunté de EvidenceRecord, ou NULL.
 */
void workspace_set_relation_evidences(
    Workspace *workspace,
    const GPtrArray *evidence_records
);

/** @brief Définit le callback de catégorisation d'une personne. */
void workspace_set_person_role_callback(Workspace *workspace,
    WorkspacePersonRoleCallback callback, gpointer user_data);
/** @brief Définit le callback de confiance d'une personne. */
void workspace_set_person_confidence_callback(Workspace *workspace,
    WorkspacePersonConfidenceCallback callback, gpointer user_data);

/**
 * @brief Affiche l'état de chargement du graphe.
 *
 * L'ancien graphe emprunté est détaché.
 *
 * @param workspace Zone de travail.
 */
void workspace_set_graph_loading(
    Workspace *workspace
);

/**
 * @brief Affiche le graphe chargé avec sa disposition persistée.
 *
 * Le Workspace emprunte graph_model et graph_layout et ne les libère jamais.
 *
 * Passer NULL pour graph_model équivaut à workspace_clear_graph().
 * Passer NULL pour graph_layout conserve le placement automatique.
 *
 * @param workspace Zone de travail.
 * @param graph_model Graphe emprunté.
 * @param graph_layout Disposition empruntée, ou NULL.
 */
void workspace_set_graph(
    Workspace *workspace,
    const InvestigationGraphModel *graph_model,
    const InvestigationGraphLayout *graph_layout
);

/**
 * @brief Affiche une erreur de chargement du graphe.
 *
 * Le texte du message est immédiatement copié par GTK.
 *
 * @param workspace Zone de travail.
 * @param message Détail de l'erreur, ou NULL.
 */
void workspace_set_graph_error(
    Workspace *workspace,
    const char *message
);

/**
 * @brief Détache le graphe et restaure l'état sans enquête.
 *
 * Le graphe précédemment emprunté n'est jamais libéré par le Workspace.
 *
 * @param workspace Zone de travail.
 */
void workspace_clear_graph(
    Workspace *workspace
);

/**
 * @brief Reconstruit visuellement la disposition automatique du graphe.
 *
 * Cette fonction ne touche ni SQLite ni InvestigationGraphLayout. Elle doit
 * être appelée par la couche applicative uniquement après la suppression
 * réussie des positions persistées.
 *
 * Le zoom et le déplacement global du canvas sont conservés.
 *
 * @param workspace Zone de travail.
 */
void workspace_reset_graph_layout(
    Workspace *workspace
);

/**
 * @brief Libère la structure Workspace.
 *
 * Cette fonction accepte NULL. Elle ne libère jamais le graphe emprunté.
 *
 * @param workspace Zone de travail à libérer.
 */
void workspace_free(
    Workspace *workspace
);

#endif
