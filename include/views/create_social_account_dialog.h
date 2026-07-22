/******************************************************************************
 * @file create_social_account_dialog.h
 * @brief Dialogue GTK de création d'un compte social observé.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_CREATE_SOCIAL_ACCOUNT_DIALOG_H
#define LABFY_INVESTIGATION_CREATE_SOCIAL_ACCOUNT_DIALOG_H

#include "models/evidence_record.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

/** @brief Résultat opaque du dialogue de compte social. */
typedef struct CreateSocialAccountDialogResult CreateSocialAccountDialogResult;

/** @brief Callback appelé après validation ou annulation. */
typedef void (*CreateSocialAccountDialogCallback)(
    CreateSocialAccountDialogResult *result,
    gpointer user_data
);

/**
 * @brief Présente le formulaire de création d'un compte social.
 *
 * Le tableau de preuves est uniquement emprunté pendant cet appel ; les
 * informations nécessaires au formulaire sont copiées.
 *
 * @param parent Fenêtre parente.
 * @param evidence_records Tableau facultatif de EvidenceRecord.
 * @param callback Callback de fin.
 * @param user_data Données privées du callback.
 * @return TRUE si le dialogue a été présenté.
 */
gboolean create_social_account_dialog_present(
    GtkWindow *parent,
    const GPtrArray *evidence_records,
    CreateSocialAccountDialogCallback callback,
    gpointer user_data
);

/** @brief Libère un résultat, y compris toutes ses chaînes. */
void create_social_account_dialog_result_free(
    CreateSocialAccountDialogResult *result
);

/** @brief Retourne le code de la plateforme sélectionnée. */
const char *create_social_account_dialog_result_get_platform(const CreateSocialAccountDialogResult *result);
/** @brief Retourne l'URL complète du profil. */
const char *create_social_account_dialog_result_get_profile_url(const CreateSocialAccountDialogResult *result);
/** @brief Retourne le pseudonyme affiché. */
const char *create_social_account_dialog_result_get_username(const CreateSocialAccountDialogResult *result);
/** @brief Retourne l'identifiant stable facultatif de la plateforme. */
const char *create_social_account_dialog_result_get_platform_identifier(const CreateSocialAccountDialogResult *result);
/** @brief Retourne la date UTC de première observation. */
const char *create_social_account_dialog_result_get_first_observed_at(const CreateSocialAccountDialogResult *result);
/** @brief Retourne l'état observé du compte. */
const char *create_social_account_dialog_result_get_account_state(const CreateSocialAccountDialogResult *result);
/** @brief Retourne les notes factuelles facultatives. */
const char *create_social_account_dialog_result_get_notes(const CreateSocialAccountDialogResult *result);
/** @brief Retourne l'UUID de la preuve associée, ou NULL. */
const char *create_social_account_dialog_result_get_evidence_identifier(const CreateSocialAccountDialogResult *result);

G_END_DECLS

#endif
