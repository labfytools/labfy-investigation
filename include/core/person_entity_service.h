/******************************************************************************
 * @file person_entity_service.h
 * @brief Création transactionnelle d'une personne observée.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_PERSON_ENTITY_SERVICE_H
#define LABFY_INVESTIGATION_PERSON_ENTITY_SERVICE_H
#include "database/database.h"
#include "models/entity_record.h"
#include <glib.h>
G_BEGIN_DECLS
/** @brief Données factuelles utilisées pour créer une personne. */
typedef struct
{
    const char *designation;
    const char *declared_name;
    const char *pseudonym;
    const char *identification_status;
    const char *notes;
    gint confidence;
    const char *evidence_identifier;
} PersonEntityInput;
/**
 * @brief Crée une personne et son éventuelle liaison à une preuve.
 * @param database Connexion ouverte empruntée.
 * @param input Données à enregistrer.
 * @param out_identifier Destination facultative de l'UUID créé.
 * @param error Destination facultative d'une erreur.
 * @return TRUE après validation complète de la transaction.
 */
gboolean person_entity_service_create(Database *database,
    const PersonEntityInput *input, char **out_identifier, GError **error);
/** @brief Enregistre le rôle d'une personne existante. */
gboolean person_entity_service_update_role(Database *database,
    const char *entity_identifier, PersonRole role, GError **error);
G_END_DECLS
#endif
