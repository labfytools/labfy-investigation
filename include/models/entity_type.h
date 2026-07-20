/******************************************************************************
 * @file entity_type.h
 * @brief Modèle représentant un type d'entité.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_ENTITY_TYPE_H
#define LABFY_INVESTIGATION_ENTITY_TYPE_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant un type d'entité.
 */
typedef struct EntityType EntityType;

/**
 * @brief Codes d'erreur produits par EntityType.
 */
typedef enum
{
    /**
     * Un argument général est invalide.
     */
    ENTITY_TYPE_ERROR_INVALID_ARGUMENT,

    /**
     * L'identifiant numérique est invalide.
     */
    ENTITY_TYPE_ERROR_INVALID_IDENTIFIER,

    /**
     * Le code métier est invalide.
     */
    ENTITY_TYPE_ERROR_INVALID_CODE,

    /**
     * Le libellé utilisateur est invalide.
     */
    ENTITY_TYPE_ERROR_INVALID_LABEL
} EntityTypeError;

/**
 * @brief Domaine d'erreur du modèle EntityType.
 */
#define ENTITY_TYPE_ERROR \
    entity_type_error_quark()

/**
 * @brief Retourne le domaine d'erreur du modèle.
 *
 * @return Quark GLib associé à EntityType.
 */
GQuark entity_type_error_quark(void);

/**
 * @brief Crée un type d'entité.
 *
 * Les chaînes sont copiées et appartiennent au nouvel objet.
 *
 * L'identifiant doit être strictement positif.
 *
 * Le code et le libellé sont obligatoires.
 * La description peut être NULL ou vide.
 *
 * @param identifier Identifiant numérique SQLite.
 * @param code Code métier du type.
 * @param label Libellé destiné à l'utilisateur.
 * @param description Description facultative.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau modèle, ou NULL lorsque les données sont invalides.
 */
EntityType *entity_type_new(
    gint64 identifier,
    const char *code,
    const char *label,
    const char *description,
    GError **error
);

/**
 * @brief Libère un modèle EntityType.
 *
 * Cette fonction accepte entity_type == NULL.
 *
 * @param entity_type Modèle à libérer.
 */
void entity_type_free(
    EntityType *entity_type
);

/**
 * @brief Retourne l'identifiant numérique du type.
 *
 * @return Identifiant, ou 0 lorsque entity_type est NULL.
 */
gint64 entity_type_get_identifier(
    const EntityType *entity_type
);

/**
 * @brief Retourne le code métier du type.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *entity_type_get_code(
    const EntityType *entity_type
);

/**
 * @brief Retourne le libellé utilisateur du type.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *entity_type_get_label(
    const EntityType *entity_type
);

/**
 * @brief Retourne la description du type.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *entity_type_get_description(
    const EntityType *entity_type
);

G_END_DECLS

#endif
