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
 * @brief Représentation opaque de la zone de travail.
 *
 * La structure réelle est définie dans workspace.c.
 * Les autres modules manipulent uniquement un pointeur vers Workspace.
 */
typedef struct Workspace Workspace;

/**
 * @brief Crée une nouvelle zone de travail.
 *
 * @return Une nouvelle zone de travail, ou NULL en cas d'échec.
 */
Workspace *workspace_new(void);

/**
 * @brief Callback appelé lorsque l'utilisateur demande la vérification
 *        de la preuve affichée.
 *
 * L'identifiant est emprunté et uniquement valide pendant l'appel.
 *
 * @param evidence_identifier UUID de la preuve.
 * @param user_data Données privées du callback.
 */
typedef void (*WorkspaceVerifyEvidenceCallback)(
    const char *evidence_identifier,
    gpointer user_data
);

/**
 * @brief Retourne le widget GTK racine de la zone de travail.
 *
 * Le widget retourné appartient au module Workspace et ne doit pas être
 * détruit directement par le code appelant.
 *
 * @param workspace Zone de travail à consulter.
 *
 * @return Le widget racine, ou NULL si workspace est NULL.
 */
GtkWidget *workspace_get_widget(
    const Workspace *workspace
);

/**
 * @brief Affiche les informations du nœud sélectionné.
 *
 * Le Workspace ne devient pas propriétaire du nœud.
 * Le nœud doit rester valide pendant toute la durée de son affichage.
 *
 * Passer NULL restaure la page d'accueil.
 *
 * @param workspace Zone de travail à mettre à jour.
 * @param node      Nœud sélectionné en lecture seule, ou NULL.
 */
void workspace_set_selected_node(
    Workspace *workspace,
    const InvestigationNode *node
);

/**
 * @brief Affiche la fiche détaillée d'une preuve.
 *
 * Les valeurs sont immédiatement copiées par les widgets GTK.
 * Le Workspace ne conserve pas le pointeur EvidenceRecord.
 *
 * Passer NULL restaure la page d'accueil.
 *
 * @param workspace Zone de travail à mettre à jour.
 * @param evidence_record Preuve sélectionnée, ou NULL.
 */
void workspace_set_selected_evidence(
    Workspace *workspace,
    const EvidenceRecord *evidence_record
);

/**
 * @brief Définit le callback de vérification d'une preuve.
 *
 * Le Workspace transmet uniquement l'identifiant de la preuve affichée.
 *
 * @param workspace Zone de travail.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void workspace_set_verify_evidence_callback(
    Workspace *workspace,
    WorkspaceVerifyEvidenceCallback callback,
    gpointer user_data
);

/**
 * @brief Libère la structure d'encapsulation de la zone de travail.
 *
 * Cette fonction accepte NULL.
 *
 * @param workspace Zone de travail à libérer.
 */
void workspace_free(Workspace *workspace);

#endif
