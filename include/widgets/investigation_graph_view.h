/******************************************************************************
 * @file investigation_graph_view.h
 * @brief Vue GTK en lecture seule du graphe d'enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_GRAPH_VIEW_H
#define LABFY_INVESTIGATION_INVESTIGATION_GRAPH_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Représentation opaque d'une entité métier.
 */
typedef struct EntityRecord EntityRecord;

/**
 * @brief Représentation opaque du graphe métier d'une enquête.
 */
typedef struct InvestigationGraphModel InvestigationGraphModel;

/**
 * @brief Représentation opaque de la disposition persistée du graphe.
 */
typedef struct InvestigationGraphLayout InvestigationGraphLayout;

/**
 * @brief Représentation opaque de la vue graphique.
 */
typedef struct InvestigationGraphView InvestigationGraphView;

/**
 * @brief Callback appelé lors d'un changement de sélection.
 *
 * entity_record est emprunté au graphe. NULL représente une désélection.
 *
 * @param entity_record Entité sélectionnée empruntée, ou NULL.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*InvestigationGraphViewSelectionCallback)(
    const EntityRecord *entity_record,
    gpointer user_data
);

/**
 * @brief Callback appelé après le déplacement effectif d'un nœud.
 *
 * Les coordonnées sont exprimées dans l'espace logique du graphe.
 * node_identifier est emprunté au modèle et reste valable uniquement
 * pendant l'appel.
 *
 * @param node_identifier UUID du nœud déplacé (entité ou relation).
 * @param x Coordonnée horizontale logique du coin supérieur gauche.
 * @param y Coordonnée verticale logique du coin supérieur gauche.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*InvestigationGraphViewNodeMovedCallback)(
    const char *node_identifier,
    double x,
    double y,
    gpointer user_data
);

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
 * @brief Associe une disposition persistée à la vue.
 *
 * La disposition est empruntée et doit rester valide pendant son utilisation.
 *
 * Lorsqu'un graphe est déjà affiché, la disposition privée de ses nœuds est
 * reconstruite. Une position persistée remplace la position automatique
 * uniquement lorsqu'elle existe pour l'entité concernée.
 *
 * Passer NULL rétablit le placement automatique.
 *
 * @param graph_view Vue graphique à mettre à jour.
 * @param graph_layout Disposition persistée empruntée, ou NULL.
 */
void investigation_graph_view_set_layout(
    InvestigationGraphView *graph_view,
    const InvestigationGraphLayout *graph_layout
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
 * @brief Définit le callback de changement de sélection.
 *
 * Le callback et user_data sont empruntés par la vue.
 *
 * @param graph_view Vue graphique.
 * @param callback Callback à appeler, ou NULL.
 * @param user_data Données empruntées du callback.
 */
void investigation_graph_view_set_selection_callback(
    InvestigationGraphView *graph_view,
    InvestigationGraphViewSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de fin de déplacement d'un nœud.
 *
 * Le callback et user_data sont empruntés par la vue.
 *
 * Le callback n'est appelé ni pendant le déplacement du canvas, ni pour
 * un simple clic, ni pendant une reconstruction de la disposition.
 *
 * @param graph_view Vue graphique.
 * @param callback Callback à appeler, ou NULL.
 * @param user_data Données empruntées du callback.
 */
void investigation_graph_view_set_node_moved_callback(
    InvestigationGraphView *graph_view,
    InvestigationGraphViewNodeMovedCallback callback,
    gpointer user_data
);

/**
 * @brief Retourne l'entité actuellement sélectionnée.
 *
 * Le pointeur retourné est emprunté au graphe.
 *
 * @param graph_view Vue graphique à consulter.
 *
 * @return Entité sélectionnée empruntée, ou NULL.
 */
const EntityRecord *investigation_graph_view_get_selected_entity(
    const InvestigationGraphView *graph_view
);

/**
 * @brief Supprime la sélection courante.
 *
 * Le callback reçoit NULL seulement si une sélection existait.
 *
 * @param graph_view Vue graphique.
 */
void investigation_graph_view_clear_selection(
    InvestigationGraphView *graph_view
);

/**
 * @brief Réinitialise le zoom et la position du canvas.
 *
 * Le graphe emprunté reste associé à la vue.
 *
 * @param graph_view Vue graphique à recentrer.
 */
void investigation_graph_view_reset_view(
    InvestigationGraphView *graph_view
);

/**
 * @brief Restaure la disposition déterministe initiale des nœuds.
 *
 * Le zoom et le déplacement global du canvas sont conservés.
 *
 * @param graph_view Vue graphique à réorganiser.
 */
void investigation_graph_view_reset_layout(
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
