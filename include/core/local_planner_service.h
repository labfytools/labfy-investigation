#ifndef LABFY_INVESTIGATION_LOCAL_PLANNER_SERVICE_H
#define LABFY_INVESTIGATION_LOCAL_PLANNER_SERVICE_H
#include "core/local_capability_registry.h"
#include "core/local_job_store.h"
#include "database/database.h"
#include <gio/gio.h>
G_BEGIN_DECLS
#define LOCAL_PLANNER_CONTRACT "labfy.local_planner.snapshot.v1"
#define LOCAL_PLANNER_RULE_VERSION "labfy.local_planner.rules.v1"
/** Projection recalculable. Elle ne lance aucun outil et ne vaut pas approbation. */
GBytes *local_planner_service_build(Database *database, LocalJobStore *store,
    const LocalCapabilityRegistry *registry, const char *investigation_id,
    gsize max_json_bytes, GError **error);
gboolean local_planner_snapshot_write_atomic(GBytes *snapshot,
    const char *path, GError **error);
G_END_DECLS
#endif
