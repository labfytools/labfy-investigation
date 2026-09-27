/******************************************************************************
 * @file local_correlation_service.h
 * @brief Index local typé et rapprochements explicables J7.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_LOCAL_CORRELATION_SERVICE_H
#define LABFY_INVESTIGATION_LOCAL_CORRELATION_SERVICE_H

#include "database/database.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define LOCAL_CORRELATION_CONTRACT "labfy.local_correlation.snapshot.v1"
#define LOCAL_CORRELATION_RULE_VERSION "labfy.local_identifier_normalization.v1"

typedef struct {
    guint max_observations;
    guint max_groups;
    guint max_connections;
    gsize max_json_bytes;
} LocalCorrelationLimits;

/** Normalise une valeur typée selon la même règle que l'index. */
char *local_correlation_normalize(const char *type, const char *raw,
    char **out_reason);

/**
 * Construit une projection possédée depuis une lecture cohérente de SQLite.
 *
 * CONTRACT: le service ne modifie ni la base métier V20 ni le JobStore. Il
 * indexe seulement `email_address`, `domain_name` et `ip_address`; les valeurs
 * invalides ou rejetées restent visibles mais ne soutiennent aucun lien.
 * Si l'appelant n'a pas de transaction active, le service possède un snapshot
 * de lecture qu'il valide lui-même. Une transaction empruntée reste intacte.
 * INVARIANT: le JSON retourné possède ses octets et doit être libéré avec
 * `g_bytes_unref()`. Les compteurs de preuves et de contenus sont distincts.
 */
GBytes *local_correlation_service_build(Database *database,
    const char *investigation_identifier, LocalCorrelationLimits limits,
    GError **error);

/** Publie atomiquement avec un temporaire unique dans le même répertoire. */
gboolean local_correlation_snapshot_write_atomic(GBytes *snapshot,
    const char *path, GError **error);

G_END_DECLS
#endif
