#ifndef LABFY_INVESTIGATION_EML_ANALYSIS_PERSISTENCE_SERVICE_TEST_H
#define LABFY_INVESTIGATION_EML_ANALYSIS_PERSISTENCE_SERVICE_TEST_H

#include <glib.h>

G_BEGIN_DECLS

/** Hook compilé uniquement par la cible de test dédiée. */
void eml_analysis_persistence_test_fail_after_derivative_insert(
    gboolean enabled);

G_END_DECLS

#endif
