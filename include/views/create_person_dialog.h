/******************************************************************************
 * @file create_person_dialog.h
 * @brief Formulaire GTK de création d'une personne observée.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_CREATE_PERSON_DIALOG_H
#define LABFY_INVESTIGATION_CREATE_PERSON_DIALOG_H
#include "core/person_entity_service.h"
#include "models/evidence_record.h"
#include <gtk/gtk.h>
G_BEGIN_DECLS
/** @brief Résultat opaque possédé par le callback. */
typedef struct CreatePersonDialogResult CreatePersonDialogResult;
/** @brief Callback appelé après validation ou annulation. */
typedef void (*CreatePersonDialogCallback)(CreatePersonDialogResult *result,
    gpointer user_data);
/**
 * @brief Présente le formulaire de création d'une personne.
 * @param parent Fenêtre parente.
 * @param evidence_records Preuves disponibles empruntées.
 * @param callback Callback de fin.
 * @param user_data Données privées du callback.
 * @return TRUE si le dialogue a été présenté.
 */
gboolean create_person_dialog_present(GtkWindow *parent,
    const GPtrArray *evidence_records, CreatePersonDialogCallback callback,
    gpointer user_data);
/** @brief Libère un résultat. */
void create_person_dialog_result_free(CreatePersonDialogResult *result);
/** @brief Retourne les données empruntées du résultat. */
const PersonEntityInput *create_person_dialog_result_get_input(
    const CreatePersonDialogResult *result);
G_END_DECLS
#endif
