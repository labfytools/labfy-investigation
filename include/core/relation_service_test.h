/******************************************************************************
 * @file relation_service_test.h
 * @brief Points d'injection réservés aux tests de RelationService.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_RELATION_SERVICE_TEST_H
#define LABFY_INVESTIGATION_RELATION_SERVICE_TEST_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Réinitialise tous les points d'injection du service.
 *
 * Disponible uniquement lorsque relation_service.c est compilé avec
 * RELATION_SERVICE_ENABLE_TEST_HOOKS.
 */
void relation_service_test_reset_hooks(void);

/**
 * @brief Simule l'échec du prochain COMMIT.
 */
void relation_service_test_set_commit_failure(
    gboolean enabled
);

/**
 * @brief Simule l'échec du prochain ROLLBACK.
 */
void relation_service_test_set_rollback_failure(
    gboolean enabled
);

G_END_DECLS

#endif
