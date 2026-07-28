#ifndef LABFY_INVESTIGATION_EVIDENCE_STAGING_H
#define LABFY_INVESTIGATION_EVIDENCE_STAGING_H

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

#define EVIDENCE_STAGING_MAX_BYTES (50U * 1024U * 1024U)

typedef struct EvidenceStaging EvidenceStaging;
typedef struct {
    char *source_path;
    char *staging_path;
    char *original_name;
    char *mime_type;
    char *suggested_type;
    char *sha256;
    char *prepared_at;
    guint64 size_bytes;
} EvidenceStagingResult;

EvidenceStaging *evidence_staging_new(GError **error);
EvidenceStaging *evidence_staging_ref(EvidenceStaging *staging);
void evidence_staging_free(EvidenceStaging *staging);
const char *evidence_staging_get_directory(const EvidenceStaging *staging);
EvidenceStagingResult *evidence_staging_prepare(EvidenceStaging *staging,
    const char *source_path, GCancellable *cancellable, GError **error);
void evidence_staging_result_free(EvidenceStagingResult *result);
gboolean evidence_staging_remove(EvidenceStaging *staging,
    const char *staging_path, GError **error);
gboolean evidence_staging_cleanup(EvidenceStaging *staging, GError **error);

G_END_DECLS
#endif
