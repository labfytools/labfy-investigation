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
/**
 * @brief Enregistre le niveau de confiance d'une personne existante.
 * @param database Connexion ouverte empruntée.
 * @param entity_identifier UUID de la personne.
 * @param confidence Valeur comprise entre 0 et 100.
 * @param error Destination facultative d'une erreur.
 * @return TRUE lorsque la modification est validée.
 */
gboolean person_entity_service_update_confidence(Database *database,
    const char *entity_identifier, gint confidence, GError **error);
/**
 * @brief Enregistre le nom affiché d'une personne existante.
 * @param database Connexion ouverte empruntée.
 * @param entity_identifier UUID de la personne.
 * @param display_name Nouveau nom non vide.
 * @param error Destination facultative d'une erreur.
 * @return TRUE lorsque la modification est validée.
 */
gboolean person_entity_service_update_display_name(Database *database,
    const char *entity_identifier, const char *display_name, GError **error);
G_END_DECLS
#endif
