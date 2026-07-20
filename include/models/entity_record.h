/******************************************************************************
 * @file entity_record.h
 * @brief Modèle représentant une entité persistée.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_ENTITY_RECORD_H
#define LABFY_INVESTIGATION_ENTITY_RECORD_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant une entité.
 */
typedef struct EntityRecord EntityRecord;

/**
 * @brief Statut métier d'une entité.
 *
 * UNKNOWN sert uniquement de valeur défensive et ne doit jamais être
 * persisté dans SQLite.
 */
typedef enum
{
    ENTITY_STATUS_UNKNOWN,
    ENTITY_STATUS_ACTIVE,
    ENTITY_STATUS_ARCHIVED,
    ENTITY_STATUS_DELETED
} EntityStatus;

/**
 * @brief Codes d'erreur produits par EntityRecord.
 */
typedef enum
{
    ENTITY_RECORD_ERROR_INVALID_ARGUMENT,
    ENTITY_RECORD_ERROR_INVALID_IDENTIFIER,
    ENTITY_RECORD_ERROR_INVALID_TYPE,
    ENTITY_RECORD_ERROR_INVALID_VALUE,
    ENTITY_RECORD_ERROR_INVALID_CONFIDENCE,
    ENTITY_RECORD_ERROR_INVALID_DATE,
    ENTITY_RECORD_ERROR_INVALID_STATUS
} EntityRecordError;

/**
 * @brief Domaine d'erreur du modèle EntityRecord.
 */
#define ENTITY_RECORD_ERROR \
    entity_record_error_quark()

/**
 * @brief Retourne le domaine d'erreur du modèle.
 */
GQuark entity_record_error_quark(void);

/**
 * @brief Crée une entité métier.
 *
 * Toutes les chaînes sont copiées.
 *
 * Les champs identifier, type_identifier, value, created_at et updated_at
 * sont obligatoires.
 *
 * label et description peuvent être NULL ou vides.
 *
 * confidence doit être compris entre 0 et 100.
 *
 * status doit être ACTIVE, ARCHIVED ou DELETED.
 *
 * @param identifier UUID de l'entité.
 * @param type_identifier Code métier du type d'entité.
 * @param value Valeur principale de l'entité.
 * @param label Libellé facultatif.
 * @param description Description facultative.
 * @param confidence Niveau de confiance compris entre 0 et 100.
 * @param created_at Date UTC de création.
 * @param updated_at Date UTC de dernière modification.
 * @param status Statut métier.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouvelle entité, ou NULL lorsque les données sont invalides.
 */
EntityRecord *entity_record_new(
    const char *identifier,
    const char *type_identifier,
    const char *value,
    const char *label,
    const char *description,
    gint confidence,
    const char *created_at,
    const char *updated_at,
    EntityStatus status,
    GError **error
);

/**
 * @brief Libère une entité.
 *
 * Cette fonction accepte NULL.
 */
void entity_record_free(
    EntityRecord *entity_record
);

/**
 * @brief Retourne l'UUID de l'entité.
 */
const char *entity_record_get_identifier(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne le code métier du type.
 */
const char *entity_record_get_type_identifier(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne la valeur principale.
 */
const char *entity_record_get_value(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne le libellé facultatif.
 */
const char *entity_record_get_label(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne la description facultative.
 */
const char *entity_record_get_description(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne le niveau de confiance.
 *
 * @return Valeur comprise entre 0 et 100, ou 0 si entity_record est NULL.
 */
gint entity_record_get_confidence(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne la date UTC de création.
 */
const char *entity_record_get_created_at(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne la date UTC de dernière modification.
 */
const char *entity_record_get_updated_at(
    const EntityRecord *entity_record
);

/**
 * @brief Retourne le statut métier.
 *
 * @return Statut ou ENTITY_STATUS_UNKNOWN si entity_record est NULL.
 */
EntityStatus entity_record_get_status(
    const EntityRecord *entity_record
);

G_END_DECLS

#endif
