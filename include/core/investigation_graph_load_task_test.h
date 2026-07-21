/******************************************************************************
 * @file investigation_graph_load_task_test.h
 * @brief Hooks déterministes réservés aux tests du chargement asynchrone.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LOAD_TASK_TEST_H
#define LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LOAD_TASK_TEST_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Réinitialise tous les hooks.
 *
 * À appeler uniquement lorsqu'aucun worker n'est encore suspendu.
 */
void investigation_graph_load_task_test_reset(void);

/**
 * @brief Configure une suspension avant l'ouverture SQLite.
 */
void investigation_graph_load_task_test_pause_before_open(void);

/**
 * @brief Attend que le worker atteigne la suspension avant ouverture.
 */
void investigation_graph_load_task_test_wait_until_before_open(void);

/**
 * @brief Libère le worker suspendu avant ouverture.
 */
void investigation_graph_load_task_test_release_before_open(void);

/**
 * @brief Configure une suspension après l'ouverture SQLite.
 */
void investigation_graph_load_task_test_pause_after_open(void);

/**
 * @brief Attend que le worker atteigne la suspension après ouverture.
 */
void investigation_graph_load_task_test_wait_until_after_open(void);

/**
 * @brief Libère le worker suspendu après ouverture.
 */
void investigation_graph_load_task_test_release_after_open(void);

/**
 * @brief Retourne le nombre de finalisations observées depuis le dernier reset.
 */
guint investigation_graph_load_task_test_get_completion_count(void);

G_END_DECLS

#endif
