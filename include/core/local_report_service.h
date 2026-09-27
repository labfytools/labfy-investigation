#ifndef LABFY_INVESTIGATION_LOCAL_REPORT_SERVICE_H
#define LABFY_INVESTIGATION_LOCAL_REPORT_SERVICE_H

#include "database/database.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define LOCAL_REPORT_CONTRACT "labfy.investigation_report.v1"
#define LOCAL_REPORT_RULE_VERSION "labfy.report_selection.v1"

typedef struct {
  guint max_selected;
  guint max_dependencies;
  guint max_events;
  gsize max_json_bytes;
} LocalReportLimits;

typedef struct {
  const char *title;
  const char *human_comment;
  const char *profile;
  const char *generated_at;
  const char *const *selected_node_ids;
  gsize selected_count;
  gboolean include_evidence;
  gboolean include_timeline;
  gboolean include_infrastructure;
} LocalReportRequest;

/**
 * Construit un document métier figé et minimisé depuis une coupe SQLite.
 * CONTRACT: les identifiants doivent exister dans la même enquête; le profil
 * minimal n'inclut ni original, ni chemin absolu, ni stdout/stderr complet.
 * INVARIANT: une transaction empruntée reste sous le contrôle de l'appelant.
 * Le GBytes retourné appartient à l'appelant.
 */
GBytes *local_report_service_build(Database *database,
    const char *investigation_identifier, const LocalReportRequest *request,
    LocalReportLimits limits, GError **error);

G_END_DECLS
#endif
