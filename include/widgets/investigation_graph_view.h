/******************************************************************************
 * @file investigation_graph_view.h
 * @brief Vue GTK en lecture seule du graphe d'enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_GRAPH_VIEW_H
#define LABFY_INVESTIGATION_INVESTIGATION_GRAPH_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Représentation opaque du graphe métier d'une enquête.
 */
typedef struct InvestigationGraphModel InvestigationGraphModel;

/**
 * @brief Représentation opaque de la vue graphique.
 */
typedef struct InvestigationGraphView InvestigationGraphView;

/**
 * @brief Crée une nouvelle vue graphique vide.
 *
 * La vue ne possède jamais le graphe qui lui est transmis.
 *
 * @return Nouvelle vue graphique, ou NULL en cas d'échec.
 */
InvestigationGraphView *investigation_graph_view_new(void);

/**
 * @brief Retourne le widget GTK racine de la vue.
 *
 * Le widget appartient à InvestigationGraphView.
 *
 * @param graph_view Vue graphique à consulter.
 *
 * @return Widget GTK racine, ou NULL.
 */
GtkWidget *investigation_graph_view_get_widget(
    const InvestigationGraphView *graph_view
);

/**
 * @brief Associe un graphe métier à la vue.
 *
 * Le graphe est emprunté et doit rester valide pendant son affichage.
 * Passer NULL équivaut à investigation_graph_view_clear().
 *
 * @param graph_view Vue graphique à mettre à jour.
 * @param graph_model Graphe emprunté, ou NULL.
 */
void investigation_graph_view_set_graph(
    InvestigationGraphView *graph_view,
    const InvestigationGraphModel *graph_model
);

/**
 * @brief Détache le graphe actuellement affiché.
 *
 * Le graphe précédemment emprunté n'est pas libéré.
 *
 * @param graph_view Vue graphique à vider.
 */
void investigation_graph_view_clear(
    InvestigationGraphView *graph_view
);

/**
 * @brief Libère la structure d'encapsulation de la vue.
 *
 * Cette fonction accepte NULL et ne libère jamais le graphe emprunté.
 *
 * @param graph_view Vue graphique à libérer.
 */
void investigation_graph_view_free(
    InvestigationGraphView *graph_view
);

G_END_DECLS

#endif
