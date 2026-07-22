/******************************************************************************
 * @file workspace.h
 * @brief Interface publique de la zone principale de travail.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_WORKSPACE_H
#define LABFY_INVESTIGATION_WORKSPACE_H

#include "core/investigation_node.h"
#include "models/evidence_record.h"

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
