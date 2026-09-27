/******************************************************************************
 * @file core_graph_projection_service.h
 * @brief Collecte cohérente du graphe cœur depuis les DAO de production.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_CORE_GRAPH_PROJECTION_SERVICE_H
#define LABFY_INVESTIGATION_CORE_GRAPH_PROJECTION_SERVICE_H

#include "database/database.h"
#include "models/core_graph_snapshot.h"

typedef struct LocalCapabilityRegistry LocalCapabilityRegistry;

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  CORE_GRAPH_PROJECTION_ERROR_INVALID_ARGUMENT,
  CORE_GRAPH_PROJECTION_ERROR_TRANSACTION,
  CORE_GRAPH_PROJECTION_ERROR_INVESTIGATION,
  CORE_GRAPH_PROJECTION_ERROR_ENTITY_RELATION,
  CORE_GRAPH_PROJECTION_ERROR_EVIDENCE,
  CORE_GRAPH_PROJECTION_ERROR_OBSERVATION,
  CORE_GRAPH_PROJECTION_ERROR_EXECUTION,
  CORE_GRAPH_PROJECTION_ERROR_MODEL
} CoreGraphProjectionError;

#define CORE_GRAPH_PROJECTION_ERROR core_graph_projection_error_quark()
GQuark core_graph_projection_error_quark(void);

/**
 * @brief Collecte un snapshot complet sous une transaction de lecture.
 *
 * La connexion est empruntée. Le snapshot retourné appartient à l'appelant.
 * CONTRACT: aucune migration, publication ou réparation n'est effectuée ; le
 * premier dépassement de limite ou échec DAO annule toute la projection.
 */
CoreGraphSnapshot *core_graph_projection_service_collect(Database *database,
                                                         CoreGraphLimits limits,
                                                         GError **error);

/** Variante J4 : emprunte un registre déjà sondé, sans lancer de processus. */
CoreGraphSnapshot *core_graph_projection_service_collect_with_registry(
    Database *database, CoreGraphLimits limits,
    const LocalCapabilityRegistry *registry, GError **error);

/** Variante J6 : les deux capabilities locales applicables sont actionnables.
 */
CoreGraphSnapshot *core_graph_projection_service_collect_operational(
    Database *database, CoreGraphLimits limits,
    const LocalCapabilityRegistry *registry, GError **error);

G_END_DECLS

#endif
