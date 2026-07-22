/******************************************************************************
 * @file osint_selection_context.h
 * @brief Contexte métier transmis aux futurs adaptateurs OSINT.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_SELECTION_CONTEXT_H
#define LABFY_INVESTIGATION_OSINT_SELECTION_CONTEXT_H

#include "models/entity_record.h"
#include "models/relation_record.h"

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * @brief Nature de la cible sélectionnée pour un outil OSINT.
 */
typedef enum
{
    OSINT_SELECTION_CONTEXT_KIND_UNKNOWN,
    OSINT_SELECTION_CONTEXT_KIND_ENTITY,
    OSINT_SELECTION_CONTEXT_KIND_RELATION
} OsintSelectionContextKind;

/**
 * @brief Contexte OSINT opaque et indépendant de GTK.
 */
typedef struct OsintSelectionContext OsintSelectionContext;

/**
 * @brief Crée un contexte depuis une entité.
 *
 * Toutes les données utiles sont copiées.
 *
 * @param entity_record Entité empruntée.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau contexte, ou NULL en cas d'échec.
 */
OsintSelectionContext *osint_selection_context_new_entity(
    const EntityRecord *entity_record,
    GError **error
);

/**
 * @brief Crée un contexte depuis une relation et ses extrémités.
 *
 * @param relation_record Relation empruntée.
 * @param source_entity Entité source empruntée.
 * @param target_entity Entité cible empruntée.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau contexte, ou NULL en cas d'échec.
 */
OsintSelectionContext *osint_selection_context_new_relation(
    const RelationRecord *relation_record,
    const EntityRecord *source_entity,
    const EntityRecord *target_entity,
    GError **error
);

/**
 * @brief Libère un contexte OSINT.
 *
 * Cette fonction accepte NULL.
 *
 * @param context Contexte à libérer.
 */
void osint_selection_context_free(OsintSelectionContext *context);

/** @brief Retourne la nature du contexte. */
OsintSelectionContextKind osint_selection_context_get_kind(
    const OsintSelectionContext *context
);

/** @brief Retourne l'UUID copié de la cible. */
const char *osint_selection_context_get_identifier(
    const OsintSelectionContext *context
);

/** @brief Retourne le type métier copié de la cible. */
const char *osint_selection_context_get_type(
    const OsintSelectionContext *context
);

/** @brief Retourne la valeur principale copiée de la cible. */
const char *osint_selection_context_get_value(
    const OsintSelectionContext *context
);

/** @brief Retourne la valeur source copiée d'une relation, ou NULL. */
const char *osint_selection_context_get_source_value(
    const OsintSelectionContext *context
);

/** @brief Retourne la valeur cible copiée d'une relation, ou NULL. */
const char *osint_selection_context_get_target_value(
    const OsintSelectionContext *context
);

G_END_DECLS

#endif
