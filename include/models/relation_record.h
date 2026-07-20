/******************************************************************************
 * @file relation_record.h
 * @brief Modèle représentant une relation persistée entre deux entités.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_RELATION_RECORD_H
#define LABFY_INVESTIGATION_RELATION_RECORD_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant une relation entre deux entités.
 */
typedef struct RelationRecord RelationRecord;

/**
 * @brief Statut métier d'une relation.
 *
 * UNKNOWN sert uniquement de valeur défensive et ne doit jamais être
 * persisté dans SQLite.
 */
typedef enum
{
    RELATION_STATUS_UNKNOWN,
    RELATION_STATUS_ACTIVE,
    RELATION_STATUS_ARCHIVED,
    RELATION_STATUS_DELETED,
    RELATION_STATUS_DISPUTED
} RelationStatus;

/**
 * @brief Codes d'erreur produits par RelationRecord.
 */
typedef enum
{
    RELATION_RECORD_ERROR_INVALID_ARGUMENT,
    RELATION_RECORD_ERROR_INVALID_IDENTIFIER,
    RELATION_RECORD_ERROR_INVALID_SOURCE_IDENTIFIER,
    RELATION_RECORD_ERROR_INVALID_TARGET_IDENTIFIER,
    RELATION_RECORD_ERROR_IDENTICAL_ENTITIES,
    RELATION_RECORD_ERROR_INVALID_TYPE,
    RELATION_RECORD_ERROR_INVALID_CONFIDENCE,
    RELATION_RECORD_ERROR_INVALID_DATE,
    RELATION_RECORD_ERROR_INVALID_STATUS
} RelationRecordError;

/**
 * @brief Domaine d'erreur du modèle RelationRecord.
 */
#define RELATION_RECORD_ERROR \
    relation_record_error_quark()

/**
 * @brief Retourne le domaine d'erreur du modèle.
 */
GQuark relation_record_error_quark(void);

/**
 * @brief Crée une relation métier.
 *
 * Toutes les chaînes sont copiées.
 *
 * identifier, source_entity_identifier, target_entity_identifier,
 * relation_type, created_at et updated_at sont obligatoires.
 *
 * label et justification peuvent être NULL ou vides.
 *
 * confidence doit être compris entre 0 et 100.
 *
 * La source et la cible doivent être différentes.
 *
 * @param identifier UUID de la relation.
 * @param source_entity_identifier UUID de l'entité source.
 * @param target_entity_identifier UUID de l'entité cible.
 * @param relation_type Type métier de la relation.
 * @param label Libellé facultatif.
 * @param justification Justification facultative.
 * @param confidence Niveau de confiance compris entre 0 et 100.
 * @param created_at Date UTC de création.
 * @param updated_at Date UTC de dernière modification.
 * @param status Statut métier.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouvelle relation, ou NULL lorsque les données sont invalides.
 */
RelationRecord *relation_record_new(
    const char *identifier,
    const char *source_entity_identifier,
    const char *target_entity_identifier,
    const char *relation_type,
    const char *label,
    const char *justification,
    gint confidence,
    const char *created_at,
    const char *updated_at,
    RelationStatus status,
    GError **error
);

/**
 * @brief Libère une relation.
 *
 * Cette fonction accepte NULL.
 */
void relation_record_free(
    RelationRecord *relation_record
);

/**
 * @brief Retourne l'UUID de la relation.
 */
const char *relation_record_get_identifier(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne l'UUID de l'entité source.
 */
const char *relation_record_get_source_entity_identifier(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne l'UUID de l'entité cible.
 */
const char *relation_record_get_target_entity_identifier(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne le type métier de la relation.
 */
const char *relation_record_get_relation_type(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne le libellé facultatif.
 */
const char *relation_record_get_label(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne la justification facultative.
 */
const char *relation_record_get_justification(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne le niveau de confiance.
 *
 * @return Valeur comprise entre 0 et 100, ou 0 si relation_record est NULL.
 */
gint relation_record_get_confidence(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne la date UTC de création.
 */
const char *relation_record_get_created_at(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne la date UTC de dernière modification.
 */
const char *relation_record_get_updated_at(
    const RelationRecord *relation_record
);

/**
 * @brief Retourne le statut métier.
 *
 * @return Statut ou RELATION_STATUS_UNKNOWN si relation_record est NULL.
 */
RelationStatus relation_record_get_status(
    const RelationRecord *relation_record
);

G_END_DECLS

#endif
