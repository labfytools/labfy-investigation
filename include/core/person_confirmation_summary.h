#ifndef LABFY_INVESTIGATION_PERSON_CONFIRMATION_SUMMARY_H
#define LABFY_INVESTIGATION_PERSON_CONFIRMATION_SUMMARY_H

#include "models/evidence_record.h"
#include <glib.h>

G_BEGIN_DECLS

char *person_confirmation_summary_build(const char *designation,
    const char *declared_name, const char *pseudonym,
    const char *identification_status, gint confidence, const char *notes,
    const GPtrArray *role_labels, const EvidenceRecord *evidence);

G_END_DECLS

#endif
