/******************************************************************************
 * @file manage_entity_evidence_dialog.h
 * @brief Dialogue de sélection des preuves associées à une entité.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_MANAGE_ENTITY_EVIDENCE_DIALOG_H
#define LABFY_INVESTIGATION_MANAGE_ENTITY_EVIDENCE_DIALOG_H
#include "models/evidence_record.h"
#include <gtk/gtk.h>
G_BEGIN_DECLS
typedef void (*ManageEntityEvidenceDialogCallback)(
    GPtrArray *selected_identifiers, gpointer user_data);
/**
 * @brief Présente la sélection multiple de preuves avec aperçu.
 * @param parent Fenêtre parente.
 * @param evidence_records Preuves disponibles empruntées.
 * @param selected_identifiers UUID déjà associés empruntés.
 * @param investigation_root_path Racine de l'enquête.
 * @param callback Callback recevant un tableau possédé, ou NULL à l'annulation.
 * @param user_data Données empruntées du callback.
 * @param error Destination facultative d'une erreur immédiate.
 * @return TRUE si le dialogue a été présenté.
 */
gboolean manage_entity_evidence_dialog_present(GtkWindow *parent,
    const GPtrArray *evidence_records, const GPtrArray *selected_identifiers,
    const char *investigation_root_path,
    ManageEntityEvidenceDialogCallback callback, gpointer user_data,
    GError **error);
G_END_DECLS
#endif
