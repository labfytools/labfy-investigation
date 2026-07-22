/******************************************************************************
 * @file evidence_metadata_dialog.h
 * @brief Dialogue de modification des métadonnées éditables d'une preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_METADATA_DIALOG_H
#define LABFY_INVESTIGATION_EVIDENCE_METADATA_DIALOG_H

#include "models/evidence_record.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

/** @brief Résultat opaque possédé du dialogue. */
typedef struct EvidenceMetadataDialogResult EvidenceMetadataDialogResult;

/** @brief Callback recevant le résultat possédé, ou NULL après annulation. */
typedef void (*EvidenceMetadataDialogCallback)(
    EvidenceMetadataDialogResult *result, gpointer user_data
);

/**
 * @brief Présente le dialogue prérempli de modification des métadonnées.
 * @param parent Fenêtre parente.
 * @param record Preuve empruntée.
 * @param evidence_types Tableau emprunté de EvidenceType.
 * @param callback Callback appelé une fois à la fermeture.
 * @param user_data Données empruntées du callback.
 * @return TRUE si la fenêtre a été créée.
 */
gboolean evidence_metadata_dialog_present(
    GtkWindow *parent,
    const EvidenceRecord *record,
    const GPtrArray *evidence_types,
    EvidenceMetadataDialogCallback callback,
    gpointer user_data
);

/** @brief Libère un résultat, ou accepte NULL. */
void evidence_metadata_dialog_result_free(EvidenceMetadataDialogResult *result);
/** @brief Retourne le code de type emprunté. */
const char *evidence_metadata_dialog_result_get_type_identifier(
    const EvidenceMetadataDialogResult *result);
/** @brief Retourne la source empruntée, ou NULL. */
const char *evidence_metadata_dialog_result_get_source(
    const EvidenceMetadataDialogResult *result);
/** @brief Retourne la description empruntée, ou NULL. */
const char *evidence_metadata_dialog_result_get_description(
    const EvidenceMetadataDialogResult *result);

G_END_DECLS
#endif
