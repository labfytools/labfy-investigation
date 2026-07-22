/******************************************************************************
 * @file social_account_service.h
 * @brief Création transactionnelle d'entités de comptes sociaux.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_SOCIAL_ACCOUNT_SERVICE_H
#define LABFY_INVESTIGATION_SOCIAL_ACCOUNT_SERVICE_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

/** @brief Données nécessaires à l'enregistrement d'un compte social. */
typedef struct
{
    const char *platform;
    const char *profile_url;
    const char *username;
    const char *platform_identifier;
    const char *first_observed_at;
    const char *account_state;
    const char *notes;
    const char *evidence_identifier;
} SocialAccountInput;

/**
 * @brief Valide et enregistre un compte social et sa preuve facultative.
 *
 * L'entité, sa fiche spécialisée et son éventuelle association à une preuve
 * sont créées dans une même transaction. L'identifiant retourné appartient à
 * l'appelant et doit être libéré avec g_free().
 *
 * @param database Connexion ouverte empruntée.
 * @param input Données du compte à créer.
 * @param out_entity_identifier Destination facultative de l'UUID créé.
 * @param error Emplacement facultatif recevant une erreur.
 * @return TRUE lorsque toutes les écritures sont validées.
 */
gboolean social_account_service_create(
    Database *database,
    const SocialAccountInput *input,
    char **out_entity_identifier,
    GError **error
);

G_END_DECLS

#endif
