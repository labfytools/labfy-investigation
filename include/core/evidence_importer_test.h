/******************************************************************************
 * @file evidence_importer_test.h
 * @brief Crochets réservés aux tests de EvidenceImporter.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_IMPORTER_TEST_H
#define LABFY_INVESTIGATION_EVIDENCE_IMPORTER_TEST_H

#include <glib.h>

G_BEGIN_DECLS

void evidence_importer_test_reset_hooks(void);

void evidence_importer_test_set_cancel_after_copy(
    gboolean enabled
);

void evidence_importer_test_set_commit_failure(
    gboolean enabled
);

void evidence_importer_test_set_cleanup_failure(
    gboolean enabled
);

G_END_DECLS

#endif
