/******************************************************************************
 * @file investigation_graph_layout.h
 * @brief Modèle en mémoire de la disposition du graphe d'enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LAYOUT_H
#define LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LAYOUT_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant la disposition du graphe.
 */
typedef struct InvestigationGraphLayout InvestigationGraphLayout;

/**
 * @brief Codes d'erreur du modèle de disposition.
 */
typedef enum
{
    INVESTIGATION_GRAPH_LAYOUT_ERROR_INVALID_ARGUMENT,
    INVESTIGATION_GRAPH_LAYOUT_ERROR_INVALID_IDENTIFIER,
    INVESTIGATION_GRAPH_LAYOUT_ERROR_INVALID_COORDINATE,
    INVESTIGATION_GRAPH_LAYOUT_ERROR_MEMORY
} InvestigationGraphLayoutError;

/**
 * @brief Domaine d'erreur du modèle de disposition.
 */
#define INVESTIGATION_GRAPH_LAYOUT_ERROR \
    investigation_graph_layout_error_quark()

/**
 * @brief Retourne le domaine d'erreur du modèle.
 */
GQuark investigation_graph_layout_error_quark(void);

/**
 * @brief Crée une disposition vide.
 *
 * @return Nouvelle disposition, ou NULL en cas d'échec.
 */
InvestigationGraphLayout *investigation_graph_layout_new(void);

/**
 * @brief Enregistre ou remplace la position d'une entité.
 *
 * L'identifiant est copié.
 *
 * @param layout Disposition à modifier.
 * @param entity_identifier UUID de l'entité.
 * @param x Coordonnée horizontale logique.
 * @param y Coordonnée verticale logique.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return TRUE si la position a été enregistrée, sinon FALSE.
 */
gboolean investigation_graph_layout_set_position(
    InvestigationGraphLayout *layout,
    const char *entity_identifier,
    double x,
    double y,
    GError **error
);

/**
 * @brief Lit la position d'une entité.
 *
 * @param layout Disposition à consulter.
 * @param entity_identifier UUID de l'entité.
 * @param x Destination facultative de la coordonnée horizontale.
 * @param y Destination facultative de la coordonnée verticale.
 *
 * @return TRUE si une position existe, sinon FALSE.
 */
gboolean investigation_graph_layout_get_position(
    const InvestigationGraphLayout *layout,
    const char *entity_identifier,
    double *x,
    double *y
);

/**
 * @brief Supprime la position d'une entité.
 *
 * L'absence de position n'est pas une erreur.
 *
 * @param layout Disposition à modifier.
 * @param entity_identifier UUID de l'entité.
 *
 * @return TRUE si une position a été supprimée, sinon FALSE.
 */
gboolean investigation_graph_layout_remove_position(
    InvestigationGraphLayout *layout,
    const char *entity_identifier
);

/**
 * @brief Supprime toutes les positions.
 *
 * @param layout Disposition à vider.
 */
void investigation_graph_layout_clear(
    InvestigationGraphLayout *layout
);

/**
 * @brief Retourne le nombre de positions conservées.
 *
 * @param layout Disposition à consulter.
 *
 * @return Nombre de positions.
 */
guint investigation_graph_layout_get_count(
    const InvestigationGraphLayout *layout
);

/**
 * @brief Libère la disposition.
 *
 * Cette fonction accepte NULL.
 *
 * @param layout Disposition à libérer.
 */
void investigation_graph_layout_free(
    InvestigationGraphLayout *layout
);

G_END_DECLS

#endif
