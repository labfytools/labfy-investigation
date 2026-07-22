/******************************************************************************
 * @file osint_action_catalog.h
 * @brief Catalogue des actions compatibles avec une sélection OSINT.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_ACTION_CATALOG_H
#define LABFY_INVESTIGATION_OSINT_ACTION_CATALOG_H

#include "models/osint_selection_context.h"

#include <glib.h>

G_BEGIN_DECLS

/** @brief Description opaque d'une action OSINT. */
typedef struct OsintAction OsintAction;

/** @brief Catalogue opaque possédant ses actions. */
typedef struct OsintActionCatalog OsintActionCatalog;

/** @brief État d'une dépendance externe vu par le catalogue OSINT. */
typedef enum
{
    OSINT_ACTION_TOOL_STATE_UNKNOWN,
    OSINT_ACTION_TOOL_STATE_AVAILABLE,
    OSINT_ACTION_TOOL_STATE_MISSING,
    OSINT_ACTION_TOOL_STATE_INCOMPATIBLE
} OsintActionToolState;

/**
 * @brief Crée le catalogue initial déterministe.
 *
 * @return Nouveau catalogue, ou NULL en cas d'échec.
 */
OsintActionCatalog *osint_action_catalog_new_defaults(void);

/**
 * @brief Libère un catalogue et toutes ses actions.
 *
 * @param catalog Catalogue à libérer, ou NULL.
 */
void osint_action_catalog_free(OsintActionCatalog *catalog);

/**
 * @brief Liste les actions compatibles avec un contexte.
 *
 * Le tableau appartient à l'appelant. Ses actions sont empruntées au catalogue
 * et triées par libellé croissant.
 *
 * @param catalog Catalogue à consulter.
 * @param context Contexte de sélection à comparer.
 *
 * @return Nouveau tableau, éventuellement vide, ou NULL si un argument est invalide.
 */
GPtrArray *osint_action_catalog_list_compatible(
    const OsintActionCatalog *catalog,
    const OsintSelectionContext *context
);

/**
 * @brief Actualise les actions dépendant d'un outil externe.
 *
 * @param catalog Catalogue à modifier.
 * @param tool_identifier Identifiant stable de l'outil.
 * @param state État traduit par la couche applicative.
 * @param version Version détectée facultative.
 */
void osint_action_catalog_update_tool_state(
    OsintActionCatalog *catalog,
    const char *tool_identifier,
    OsintActionToolState state,
    const char *version
);

/** @brief Retourne l'identifiant stable emprunté d'une action. */
const char *osint_action_get_identifier(const OsintAction *action);

/** @brief Retourne le libellé emprunté d'une action. */
const char *osint_action_get_label(const OsintAction *action);

/** @brief Retourne la description empruntée d'une action. */
const char *osint_action_get_description(const OsintAction *action);

/** @brief Retourne l'identifiant de l'outil externe requis, ou NULL. */
const char *osint_action_get_required_tool_identifier(
    const OsintAction *action
);

/** @brief Indique si l'action peut actuellement être déclenchée. */
gboolean osint_action_is_available(const OsintAction *action);

/** @brief Retourne l'explication d'indisponibilité, ou NULL. */
const char *osint_action_get_unavailable_reason(const OsintAction *action);

/** @brief Retourne la version copiée de l'outil requis, ou NULL. */
const char *osint_action_get_tool_version(const OsintAction *action);

G_END_DECLS

#endif
