/******************************************************************************
 * @file investigation_graph_view.c
 * @brief Vue GTK en lecture seule du graphe d'enquête.
 ******************************************************************************/

#include "widgets/investigation_graph_view.h"

#include "models/entity_record.h"
#include "models/investigation_graph_layout.h"
#include "models/investigation_graph_model.h"
#include "models/relation_record.h"
#include "models/social_platform.h"

#include <glib.h>
#include <math.h>
#include <pango/pangocairo.h>

#define INVESTIGATION_GRAPH_VIEW_MARGIN 32.0
#define INVESTIGATION_GRAPH_VIEW_NODE_WIDTH 190.0
#define INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT 78.0
#define INVESTIGATION_GRAPH_VIEW_RELATION_NODE_WIDTH 142.0
#define INVESTIGATION_GRAPH_VIEW_RELATION_NODE_HEIGHT 30.0
#define INVESTIGATION_GRAPH_VIEW_HORIZONTAL_GAP 28.0
#define INVESTIGATION_GRAPH_VIEW_VERTICAL_GAP 28.0
#define INVESTIGATION_GRAPH_VIEW_NODE_RADIUS 12.0
#define INVESTIGATION_GRAPH_VIEW_RELATION_NODE_RADIUS 10.0
#define INVESTIGATION_GRAPH_VIEW_NODE_PADDING 12
#define INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_SIZE 38.0
#define INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_GAP 10.0
#define INVESTIGATION_GRAPH_VIEW_RELATION_NODE_GAP 38.0
#define INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH 9.0
#define INVESTIGATION_GRAPH_VIEW_ARROW_HALF_WIDTH 5.0

#define INVESTIGATION_GRAPH_VIEW_MIN_ZOOM 0.25
#define INVESTIGATION_GRAPH_VIEW_MAX_ZOOM 3.0
#define INVESTIGATION_GRAPH_VIEW_SCROLL_ZOOM_SPEED 0.08
#define INVESTIGATION_GRAPH_VIEW_MIN_SCROLL_FACTOR 0.50
#define INVESTIGATION_GRAPH_VIEW_MAX_SCROLL_FACTOR 1.50
#define INVESTIGATION_GRAPH_VIEW_CLICK_TOLERANCE 4.0

/**
 * @brief Nature d'un nœud visible dans le graphe.
 */
typedef enum
{
    INVESTIGATION_GRAPH_NODE_KIND_ENTITY,
    INVESTIGATION_GRAPH_NODE_KIND_RELATION
} InvestigationGraphNodeKind;

/**
 * @brief Position privée d'un nœud dans la vue.
 */
typedef struct
{
    InvestigationGraphNodeKind kind;

    const EntityRecord *entity_record;
    const RelationRecord *relation_record;

    double x;
    double y;

    double width;
    double height;

    double relation_offset_x;
    double relation_offset_y;
} InvestigationGraphNodeLayout;

/**
 * @struct InvestigationGraphView
 * @brief État privé de la vue graphique.
 */
struct InvestigationGraphView
{
    GtkWidget *drawing_area;

    GtkGesture *zoom_gesture;
    GtkGesture *drag_gesture;
    GtkGesture *click_gesture;

    GtkEventController *scroll_controller;
    GtkEventController *motion_controller;

    const InvestigationGraphModel *graph_model;
    const InvestigationGraphLayout *graph_layout;

    GPtrArray *node_layouts;
    GHashTable *node_layouts_by_identifier;

    GPtrArray *relations;
    GHashTable *relation_pair_counts;

    char *layout_error_message;

    double zoom;
    double offset_x;
    double offset_y;

    double zoom_gesture_start_zoom;
    double zoom_gesture_start_offset_x;
    double zoom_gesture_start_offset_y;
    double zoom_gesture_anchor_x;
    double zoom_gesture_anchor_y;

    double pointer_x;
    double pointer_y;

    double drag_start_x;
    double drag_start_y;

    double drag_origin_offset_x;
    double drag_origin_offset_y;

    InvestigationGraphNodeLayout *dragged_node;
    InvestigationGraphNodeLayout *selected_node;
    InvestigationGraphNodeLayout *click_candidate_node;

    InvestigationGraphViewSelectionCallback selection_callback;
    gpointer selection_user_data;

    InvestigationGraphViewNodeMovedCallback node_moved_callback;
    gpointer node_moved_user_data;
    InvestigationGraphViewExtractionDropCallback extraction_drop_callback;
    gpointer extraction_drop_user_data;

    double node_drag_pointer_offset_x;
    double node_drag_pointer_offset_y;

    gboolean pointer_position_valid;
    gboolean dragging;
    gboolean node_dragging;
    gboolean click_suppressed;
    GtkDropTarget *extraction_drop_target;
};

static gboolean investigation_graph_view_screen_to_logical(
    const InvestigationGraphView *graph_view, double screen_x, double screen_y,
    double *logical_x, double *logical_y);
static InvestigationGraphNodeLayout *investigation_graph_view_find_node_at(
    InvestigationGraphView *graph_view, double logical_x, double logical_y);

/** @brief Relaie un chemin d'extraction et l'éventuelle entité visée. */
static gboolean investigation_graph_view_on_extraction_drop(
    GtkDropTarget *target, const GValue *value, double x, double y,
    gpointer user_data)
{
    InvestigationGraphView *graph_view = user_data;
    InvestigationGraphNodeLayout *node_layout = NULL;
    const char *path = NULL;
    const char *entity_identifier = NULL;
    double logical_x = 0.0;
    double logical_y = 0.0;
    (void) target;
    if (graph_view == NULL || value == NULL ||
        !G_VALUE_HOLDS_STRING(value)) return FALSE;
    path = g_value_get_string(value);
    if (path == NULL || path[0] == '\0') return FALSE;
    if (investigation_graph_view_screen_to_logical(graph_view, x, y,
            &logical_x, &logical_y))
    {
        node_layout = investigation_graph_view_find_node_at(graph_view,
            logical_x, logical_y);
        if (node_layout != NULL && node_layout->entity_record != NULL)
            entity_identifier = entity_record_get_identifier(
                node_layout->entity_record);
    }
    if (graph_view->extraction_drop_callback == NULL) return FALSE;
    graph_view->extraction_drop_callback(path, entity_identifier,
        graph_view->extraction_drop_user_data);
    return TRUE;
}

static const char *investigation_graph_view_get_node_identifier(
    const InvestigationGraphNodeLayout *node_layout
);
static double investigation_graph_view_get_node_center_x(
    const InvestigationGraphNodeLayout *node_layout
);
static double investigation_graph_view_get_node_center_y(
    const InvestigationGraphNodeLayout *node_layout
);

/** @brief Replace les libellés de relation entre leurs extrémités. */
static void investigation_graph_view_sync_relation_layouts(
    InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL || graph_view->node_layouts == NULL ||
        graph_view->node_layouts_by_identifier == NULL)
    {
        return;
    }

    for (guint i = 0; i < graph_view->node_layouts->len; i++)
    {
        InvestigationGraphNodeLayout *relation_layout =
            g_ptr_array_index(graph_view->node_layouts, i);
        const RelationRecord *relation_record = NULL;
        InvestigationGraphNodeLayout *source_layout = NULL;
        InvestigationGraphNodeLayout *target_layout = NULL;

        if (relation_layout == NULL || relation_layout->kind !=
            INVESTIGATION_GRAPH_NODE_KIND_RELATION)
        {
            continue;
        }
        relation_record = relation_layout->relation_record;
        source_layout = g_hash_table_lookup(
            graph_view->node_layouts_by_identifier,
            relation_record_get_source_entity_identifier(relation_record));
        target_layout = g_hash_table_lookup(
            graph_view->node_layouts_by_identifier,
            relation_record_get_target_entity_identifier(relation_record));
        if (source_layout == NULL || target_layout == NULL)
        {
            continue;
        }
        relation_layout->x =
            (investigation_graph_view_get_node_center_x(source_layout) +
             investigation_graph_view_get_node_center_x(target_layout)) / 2.0 -
            relation_layout->width / 2.0 + relation_layout->relation_offset_x;
        relation_layout->y =
            (investigation_graph_view_get_node_center_y(source_layout) +
             investigation_graph_view_get_node_center_y(target_layout)) / 2.0 -
            relation_layout->height / 2.0 + relation_layout->relation_offset_y;
    }
}

/**
 * @brief Borne un facteur de zoom aux limites autorisées.
 */
static double investigation_graph_view_clamp_zoom(
    double zoom
)
{
    if (zoom < INVESTIGATION_GRAPH_VIEW_MIN_ZOOM)
    {
        return INVESTIGATION_GRAPH_VIEW_MIN_ZOOM;
    }

    if (zoom > INVESTIGATION_GRAPH_VIEW_MAX_ZOOM)
    {
        return INVESTIGATION_GRAPH_VIEW_MAX_ZOOM;
    }

    return zoom;
}

/**
 * @brief Applique un zoom en conservant un point d'écran immobile.
 */
static void investigation_graph_view_zoom_at_point(
    InvestigationGraphView *graph_view,
    double requested_zoom,
    double anchor_x,
    double anchor_y
)
{
    double previous_zoom =
        0.0;

    double logical_x =
        0.0;

    double logical_y =
        0.0;

    double new_zoom =
        0.0;

    if (graph_view == NULL ||
        graph_view->drawing_area == NULL)
    {
        return;
    }

    previous_zoom =
        graph_view->zoom;

    if (previous_zoom <= 0.0)
    {
        previous_zoom =
            1.0;
    }

    new_zoom =
        investigation_graph_view_clamp_zoom(
            requested_zoom
        );

    logical_x =
        (
            anchor_x -
            graph_view->offset_x
        ) /
        previous_zoom;

    logical_y =
        (
            anchor_y -
            graph_view->offset_y
        ) /
        previous_zoom;

    graph_view->zoom =
        new_zoom;

    graph_view->offset_x =
        anchor_x -
        (logical_x * new_zoom);

    graph_view->offset_y =
        anchor_y -
        (logical_y * new_zoom);

    gtk_widget_queue_draw(
        graph_view->drawing_area
    );
}

/**
 * @brief Mémorise la dernière position connue du pointeur.
 */
static void investigation_graph_view_on_pointer_motion(
    GtkEventControllerMotion *controller,
    double x,
    double y,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    (void) controller;

    if (graph_view == NULL)
    {
        return;
    }

    graph_view->pointer_x =
        x;

    graph_view->pointer_y =
        y;

    graph_view->pointer_position_valid =
        TRUE;
}

/**
 * @brief Oublie la position du pointeur lorsqu'il quitte le canvas.
 */
static void investigation_graph_view_on_pointer_leave(
    GtkEventControllerMotion *controller,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    (void) controller;

    if (graph_view == NULL)
    {
        return;
    }

    graph_view->pointer_position_valid =
        FALSE;
}

/**
 * @brief Mémorise l'état initial d'un pincement.
 */
static void investigation_graph_view_on_zoom_begin(
    GtkGesture *gesture,
    GdkEventSequence *sequence,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    double anchor_x =
        0.0;

    double anchor_y =
        0.0;

    gboolean center_available =
        FALSE;

    (void) sequence;

    if (graph_view == NULL ||
        graph_view->drawing_area == NULL)
    {
        return;
    }

    center_available =
        gtk_gesture_get_bounding_box_center(
            gesture,
            &anchor_x,
            &anchor_y
        );

    if (!center_available)
    {
        anchor_x =
            (double) gtk_widget_get_width(
                graph_view->drawing_area
            ) /
            2.0;

        anchor_y =
            (double) gtk_widget_get_height(
                graph_view->drawing_area
            ) /
            2.0;
    }

    graph_view->zoom_gesture_start_zoom =
        graph_view->zoom;

    graph_view->zoom_gesture_start_offset_x =
        graph_view->offset_x;

    graph_view->zoom_gesture_start_offset_y =
        graph_view->offset_y;

    graph_view->zoom_gesture_anchor_x =
        anchor_x;

    graph_view->zoom_gesture_anchor_y =
        anchor_y;
}

/**
 * @brief Applique le facteur cumulé transmis par GtkGestureZoom.
 */
static void investigation_graph_view_on_scale_changed(
    GtkGestureZoom *gesture,
    double scale,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    double start_zoom =
        0.0;

    double new_zoom =
        0.0;

    double logical_x =
        0.0;

    double logical_y =
        0.0;

    (void) gesture;

    if (graph_view == NULL ||
        graph_view->drawing_area == NULL ||
        scale <= 0.0)
    {
        return;
    }

    start_zoom =
        graph_view->zoom_gesture_start_zoom;

    if (start_zoom <= 0.0)
    {
        start_zoom =
            1.0;
    }

    new_zoom =
        investigation_graph_view_clamp_zoom(
            start_zoom * scale
        );

    logical_x =
        (
            graph_view->zoom_gesture_anchor_x -
            graph_view->zoom_gesture_start_offset_x
        ) /
        start_zoom;

    logical_y =
        (
            graph_view->zoom_gesture_anchor_y -
            graph_view->zoom_gesture_start_offset_y
        ) /
        start_zoom;

    graph_view->zoom =
        new_zoom;

    graph_view->offset_x =
        graph_view->zoom_gesture_anchor_x -
        (logical_x * new_zoom);

    graph_view->offset_y =
        graph_view->zoom_gesture_anchor_y -
        (logical_y * new_zoom);

    gtk_widget_queue_draw(
        graph_view->drawing_area
    );
}

/**
 * @brief Gère Ctrl + défilement comme solution de zoom de secours.
 */
static gboolean investigation_graph_view_on_scroll(
    GtkEventControllerScroll *controller,
    double delta_x,
    double delta_y,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    GdkModifierType modifier_state =
        0;

    double anchor_x =
        0.0;

    double anchor_y =
        0.0;

    double zoom_factor =
        1.0;

    (void) delta_x;

    if (graph_view == NULL ||
        graph_view->drawing_area == NULL ||
        delta_y == 0.0)
    {
        return FALSE;
    }

    modifier_state =
        gtk_event_controller_get_current_event_state(
            GTK_EVENT_CONTROLLER(
                controller
            )
        );

    if ((modifier_state & GDK_CONTROL_MASK) == 0)
    {
        return FALSE;
    }

    if (graph_view->pointer_position_valid)
    {
        anchor_x =
            graph_view->pointer_x;

        anchor_y =
            graph_view->pointer_y;
    }
    else
    {
        anchor_x =
            (double) gtk_widget_get_width(
                graph_view->drawing_area
            ) /
            2.0;

        anchor_y =
            (double) gtk_widget_get_height(
                graph_view->drawing_area
            ) /
            2.0;
    }

    zoom_factor =
        1.0 -
        (
            delta_y *
            INVESTIGATION_GRAPH_VIEW_SCROLL_ZOOM_SPEED
        );

    if (zoom_factor <
        INVESTIGATION_GRAPH_VIEW_MIN_SCROLL_FACTOR)
    {
        zoom_factor =
            INVESTIGATION_GRAPH_VIEW_MIN_SCROLL_FACTOR;
    }

    if (zoom_factor >
        INVESTIGATION_GRAPH_VIEW_MAX_SCROLL_FACTOR)
    {
        zoom_factor =
            INVESTIGATION_GRAPH_VIEW_MAX_SCROLL_FACTOR;
    }

    investigation_graph_view_zoom_at_point(
        graph_view,
        graph_view->zoom * zoom_factor,
        anchor_x,
        anchor_y
    );

    return TRUE;
}

/**
 * @brief Convertit une position écran en coordonnées logiques du graphe.
 */
static gboolean investigation_graph_view_screen_to_logical(
    const InvestigationGraphView *graph_view,
    double screen_x,
    double screen_y,
    double *logical_x,
    double *logical_y
)
{
    if (graph_view == NULL ||
        logical_x == NULL ||
        logical_y == NULL ||
        graph_view->zoom <= 0.0)
    {
        return FALSE;
    }

    *logical_x =
        (
            screen_x -
            graph_view->offset_x
        ) /
        graph_view->zoom;

    *logical_y =
        (
            screen_y -
            graph_view->offset_y
        ) /
        graph_view->zoom;

    return TRUE;
}

/**
 * @brief Recherche le nœud situé sous une position logique.
 *
 * La recherche est effectuée depuis la fin du tableau afin de privilégier
 * les nœuds dessinés au premier plan.
 */
static InvestigationGraphNodeLayout *
investigation_graph_view_find_node_at(
    InvestigationGraphView *graph_view,
    double logical_x,
    double logical_y
)
{
    guint layout_position =
        0;

    if (graph_view == NULL ||
        graph_view->node_layouts == NULL)
    {
        return NULL;
    }

    investigation_graph_view_sync_relation_layouts(graph_view);

    layout_position =
        graph_view->node_layouts->len;

    while (layout_position > 0)
    {
        InvestigationGraphNodeLayout *node_layout =
            NULL;

        layout_position--;

        node_layout =
            g_ptr_array_index(
                graph_view->node_layouts,
                layout_position
            );

        if (node_layout == NULL)
        {
            continue;
        }

        if (logical_x >= node_layout->x &&
            logical_x <=
                node_layout->x +
                node_layout->width &&
            logical_y >= node_layout->y &&
            logical_y <=
                node_layout->y +
                node_layout->height)
        {
            return node_layout;
        }
    }

    /* Rend également l'arête sélectionnable, avec une tolérance visuelle. */
    for (layout_position = graph_view->node_layouts->len;
         layout_position > 0; layout_position--)
    {
        InvestigationGraphNodeLayout *relation_layout =
            g_ptr_array_index(graph_view->node_layouts, layout_position - 1U);
        const RelationRecord *relation_record = NULL;
        InvestigationGraphNodeLayout *source_layout = NULL;
        InvestigationGraphNodeLayout *target_layout = NULL;
        double source_x = 0.0, source_y = 0.0;
        double target_x = 0.0, target_y = 0.0;
        double delta_x = 0.0, delta_y = 0.0, length_squared = 0.0;
        double projection = 0.0, nearest_x = 0.0, nearest_y = 0.0;
        double distance_x = 0.0, distance_y = 0.0;
        double tolerance = 7.0 / graph_view->zoom;

        if (relation_layout == NULL || relation_layout->kind !=
            INVESTIGATION_GRAPH_NODE_KIND_RELATION)
        {
            continue;
        }
        relation_record = relation_layout->relation_record;
        source_layout = g_hash_table_lookup(
            graph_view->node_layouts_by_identifier,
            relation_record_get_source_entity_identifier(relation_record));
        target_layout = g_hash_table_lookup(
            graph_view->node_layouts_by_identifier,
            relation_record_get_target_entity_identifier(relation_record));
        if (source_layout == NULL || target_layout == NULL)
        {
            continue;
        }
        source_x = investigation_graph_view_get_node_center_x(source_layout);
        source_y = investigation_graph_view_get_node_center_y(source_layout);
        target_x = investigation_graph_view_get_node_center_x(target_layout);
        target_y = investigation_graph_view_get_node_center_y(target_layout);
        delta_x = target_x - source_x;
        delta_y = target_y - source_y;
        length_squared = delta_x * delta_x + delta_y * delta_y;
        if (length_squared <= 0.0)
        {
            continue;
        }
        projection = ((logical_x - source_x) * delta_x +
            (logical_y - source_y) * delta_y) / length_squared;
        projection = CLAMP(projection, 0.0, 1.0);
        nearest_x = source_x + projection * delta_x;
        nearest_y = source_y + projection * delta_y;
        distance_x = logical_x - nearest_x;
        distance_y = logical_y - nearest_y;
        if (distance_x * distance_x + distance_y * distance_y <=
            tolerance * tolerance)
        {
            return relation_layout;
        }
    }

    return NULL;
}

/**
 * @brief Modifie la sélection et prévient l'appelant si elle change.
 */
static void investigation_graph_view_set_selected_node(
    InvestigationGraphView *graph_view,
    InvestigationGraphNodeLayout *node_layout
)
{
    const EntityRecord *entity_record =
        NULL;

    const RelationRecord *relation_record =
        NULL;

    if (graph_view == NULL ||
        graph_view->selected_node == node_layout)
    {
        return;
    }

    graph_view->selected_node =
        node_layout;

    if (node_layout != NULL &&
        node_layout->kind ==
            INVESTIGATION_GRAPH_NODE_KIND_ENTITY)
    {
        entity_record =
            node_layout->entity_record;
    }
    else if (node_layout != NULL &&
             node_layout->kind ==
                INVESTIGATION_GRAPH_NODE_KIND_RELATION)
    {
        relation_record =
            node_layout->relation_record;
    }

    if (graph_view->drawing_area != NULL)
    {
        gtk_widget_queue_draw(
            graph_view->drawing_area
        );
    }

    if (graph_view->selection_callback != NULL)
    {
        graph_view->selection_callback(
            entity_record,
            relation_record,
            graph_view->selection_user_data
        );
    }
}

/**
 * @brief Mémorise le nœud présent sous le début d'un clic.
 */
static void investigation_graph_view_on_click_pressed(
    GtkGestureClick *gesture,
    int press_count,
    double x,
    double y,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    GdkModifierType modifier_state =
        0;

    double logical_x =
        0.0;

    double logical_y =
        0.0;

    if (graph_view == NULL ||
        press_count != 1)
    {
        return;
    }

    graph_view->click_suppressed =
        FALSE;

    graph_view->click_candidate_node =
        NULL;

    modifier_state =
        gtk_event_controller_get_current_event_state(
            GTK_EVENT_CONTROLLER(
                gesture
            )
        );

    if ((modifier_state & GDK_SHIFT_MASK) != 0)
    {
        return;
    }

    if (!investigation_graph_view_screen_to_logical(
            graph_view,
            x,
            y,
            &logical_x,
            &logical_y
        ))
    {
        return;
    }

    graph_view->click_candidate_node =
        investigation_graph_view_find_node_at(
            graph_view,
            logical_x,
            logical_y
        );
}

/**
 * @brief Sélectionne le nœud cliqué ou désélectionne dans le vide.
 */
static void investigation_graph_view_on_click_released(
    GtkGestureClick *gesture,
    int press_count,
    double x,
    double y,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    GdkModifierType modifier_state =
        0;

    InvestigationGraphNodeLayout *released_node =
        NULL;

    double logical_x =
        0.0;

    double logical_y =
        0.0;

    if (graph_view == NULL ||
        press_count != 1)
    {
        return;
    }

    modifier_state =
        gtk_event_controller_get_current_event_state(
            GTK_EVENT_CONTROLLER(
                gesture
            )
        );

    if (graph_view->click_suppressed ||
        (modifier_state & GDK_SHIFT_MASK) != 0)
    {
        graph_view->click_suppressed =
            FALSE;

        graph_view->click_candidate_node =
            NULL;

        return;
    }

    if (investigation_graph_view_screen_to_logical(
            graph_view,
            x,
            y,
            &logical_x,
            &logical_y
        ))
    {
        released_node =
            investigation_graph_view_find_node_at(
                graph_view,
                logical_x,
                logical_y
            );
    }

    if (released_node ==
        graph_view->click_candidate_node)
    {
        investigation_graph_view_set_selected_node(
            graph_view,
            released_node
        );
    }

    graph_view->click_candidate_node =
        NULL;
}

/**
 * @brief Annule les états de déplacement sans modifier les positions.
 */
static void investigation_graph_view_cancel_drag(
    InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->dragged_node =
        NULL;

    graph_view->node_drag_pointer_offset_x =
        0.0;

    graph_view->node_drag_pointer_offset_y =
        0.0;

    graph_view->dragging =
        FALSE;

    graph_view->node_dragging =
        FALSE;

    if (graph_view->drawing_area != NULL)
    {
        gtk_widget_set_cursor_from_name(
            graph_view->drawing_area,
            "default"
        );
    }
}

/**
 * @brief Commence un déplacement du canvas ou d'un nœud.
 */
static void investigation_graph_view_on_drag_begin(
    GtkGestureDrag *gesture,
    double start_x,
    double start_y,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    GdkModifierType modifier_state =
        0;

    double logical_x =
        0.0;

    double logical_y =
        0.0;

    InvestigationGraphNodeLayout *node_layout =
        NULL;

    if (graph_view == NULL ||
        graph_view->drawing_area == NULL)
    {
        return;
    }

    investigation_graph_view_cancel_drag(
        graph_view
    );

    modifier_state =
        gtk_event_controller_get_current_event_state(
            GTK_EVENT_CONTROLLER(
                gesture
            )
        );

    graph_view->drag_start_x =
        start_x;

    graph_view->drag_start_y =
        start_y;

    if ((modifier_state & GDK_SHIFT_MASK) != 0)
    {
        graph_view->click_suppressed =
            TRUE;

        graph_view->click_candidate_node =
            NULL;

        graph_view->drag_origin_offset_x =
            graph_view->offset_x;

        graph_view->drag_origin_offset_y =
            graph_view->offset_y;

        graph_view->dragging =
            TRUE;

        gtk_gesture_set_state(
            GTK_GESTURE(
                gesture
            ),
            GTK_EVENT_SEQUENCE_CLAIMED
        );

        gtk_widget_set_cursor_from_name(
            graph_view->drawing_area,
            "grabbing"
        );

        return;
    }

    if (!investigation_graph_view_screen_to_logical(
            graph_view,
            start_x,
            start_y,
            &logical_x,
            &logical_y
        ))
    {
        return;
    }

    node_layout =
        investigation_graph_view_find_node_at(
            graph_view,
            logical_x,
            logical_y
        );

    if (node_layout == NULL)
    {
        return;
    }

    /* Le libellé d'une relation suit automatiquement ses deux extrémités. */
    if (node_layout->kind == INVESTIGATION_GRAPH_NODE_KIND_RELATION)
    {
        return;
    }

    graph_view->dragged_node =
        node_layout;

    graph_view->node_drag_pointer_offset_x =
        logical_x -
        node_layout->x;

    graph_view->node_drag_pointer_offset_y =
        logical_y -
        node_layout->y;

    graph_view->node_dragging =
        TRUE;

    gtk_gesture_set_state(
        GTK_GESTURE(
            gesture
        ),
        GTK_EVENT_SEQUENCE_CLAIMED
    );

    gtk_widget_set_cursor_from_name(
        graph_view->drawing_area,
        "grabbing"
    );
}

/**
 * @brief Met à jour le déplacement actif.
 */
static void investigation_graph_view_on_drag_update(
    GtkGestureDrag *gesture,
    double offset_x,
    double offset_y,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    double pointer_x =
        0.0;

    double pointer_y =
        0.0;

    double logical_x =
        0.0;

    double logical_y =
        0.0;

    (void) gesture;

    if (graph_view == NULL ||
        graph_view->drawing_area == NULL)
    {
        return;
    }

    if (
        (offset_x * offset_x) +
        (offset_y * offset_y) >
        (
            INVESTIGATION_GRAPH_VIEW_CLICK_TOLERANCE *
            INVESTIGATION_GRAPH_VIEW_CLICK_TOLERANCE
        )
    )
    {
        graph_view->click_suppressed =
            TRUE;

        graph_view->click_candidate_node =
            NULL;
    }

    if (graph_view->dragging)
    {
        graph_view->offset_x =
            graph_view->drag_origin_offset_x +
            offset_x;

        graph_view->offset_y =
            graph_view->drag_origin_offset_y +
            offset_y;

        gtk_widget_queue_draw(
            graph_view->drawing_area
        );

        return;
    }

    if (!graph_view->node_dragging ||
        graph_view->dragged_node == NULL)
    {
        return;
    }

    pointer_x =
        graph_view->drag_start_x +
        offset_x;

    pointer_y =
        graph_view->drag_start_y +
        offset_y;

    if (!investigation_graph_view_screen_to_logical(
            graph_view,
            pointer_x,
            pointer_y,
            &logical_x,
            &logical_y
        ))
    {
        return;
    }

    graph_view->dragged_node->x =
        logical_x -
        graph_view->node_drag_pointer_offset_x;

    graph_view->dragged_node->y =
        logical_y -
        graph_view->node_drag_pointer_offset_y;

    investigation_graph_view_sync_relation_layouts(graph_view);

    gtk_widget_queue_draw(
        graph_view->drawing_area
    );
}

/**
 * @brief Prévient l'appelant qu'un nœud a été déplacé.
 */
static void investigation_graph_view_notify_node_moved(
    InvestigationGraphView *graph_view,
    const InvestigationGraphNodeLayout *node_layout
)
{
    const char *node_identifier =
        NULL;

    if (graph_view == NULL ||
        node_layout == NULL ||
        graph_view->node_moved_callback == NULL)
    {
        return;
    }

    node_identifier =
        investigation_graph_view_get_node_identifier(
            node_layout
        );

    if (node_identifier == NULL ||
        node_identifier[0] == '\0')
    {
        return;
    }

    graph_view->node_moved_callback(
        node_identifier,
        node_layout->x,
        node_layout->y,
        graph_view->node_moved_user_data
    );
}

/**
 * @brief Termine le déplacement actif.
 */
static void investigation_graph_view_on_drag_end(
    GtkGestureDrag *gesture,
    double offset_x,
    double offset_y,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    double pointer_x =
        0.0;

    double pointer_y =
        0.0;

    double logical_x =
        0.0;

    double logical_y =
        0.0;

    InvestigationGraphNodeLayout *moved_node =
        NULL;

    gboolean node_was_moved =
        FALSE;

    (void) gesture;

    if (graph_view == NULL ||
        graph_view->drawing_area == NULL)
    {
        return;
    }

    if (graph_view->dragging)
    {
        graph_view->offset_x =
            graph_view->drag_origin_offset_x +
            offset_x;

        graph_view->offset_y =
            graph_view->drag_origin_offset_y +
            offset_y;
    }
    else if (graph_view->node_dragging &&
             graph_view->dragged_node != NULL)
    {
        moved_node =
            graph_view->dragged_node;

        node_was_moved =
            (
                (offset_x * offset_x) +
                (offset_y * offset_y)
            ) >
            (
                INVESTIGATION_GRAPH_VIEW_CLICK_TOLERANCE *
                INVESTIGATION_GRAPH_VIEW_CLICK_TOLERANCE
            );

        pointer_x =
            graph_view->drag_start_x +
            offset_x;

        pointer_y =
            graph_view->drag_start_y +
            offset_y;

        if (investigation_graph_view_screen_to_logical(
                graph_view,
                pointer_x,
                pointer_y,
                &logical_x,
                &logical_y
            ))
        {
            graph_view->dragged_node->x =
                logical_x -
                graph_view->node_drag_pointer_offset_x;

            graph_view->dragged_node->y =
                logical_y -
                graph_view->node_drag_pointer_offset_y;

            investigation_graph_view_sync_relation_layouts(graph_view);
        }
    }

    investigation_graph_view_cancel_drag(
        graph_view
    );

    if (node_was_moved &&
        moved_node != NULL)
    {
        investigation_graph_view_notify_node_moved(
            graph_view,
            moved_node
        );
    }

    gtk_widget_queue_draw(
        graph_view->drawing_area
    );
}

/**
 * @brief Retourne l'identifiant métier d'un nœud.
 *
 * La chaîne retournée est empruntée au modèle.
 */
static const char *investigation_graph_view_get_node_identifier(
    const InvestigationGraphNodeLayout *node_layout
)
{
    if (node_layout == NULL)
    {
        return NULL;
    }

    if (node_layout->kind ==
        INVESTIGATION_GRAPH_NODE_KIND_ENTITY)
    {
        return entity_record_get_identifier(
            node_layout->entity_record
        );
    }

    if (node_layout->kind ==
        INVESTIGATION_GRAPH_NODE_KIND_RELATION)
    {
        return relation_record_get_identifier(
            node_layout->relation_record
        );
    }

    return NULL;
}

/**
 * @brief Retourne le centre horizontal d'un nœud.
 */
static double investigation_graph_view_get_node_center_x(
    const InvestigationGraphNodeLayout *node_layout
)
{
    return node_layout != NULL
        ? node_layout->x + (node_layout->width / 2.0)
        : 0.0;
}

/**
 * @brief Retourne le centre vertical d'un nœud.
 */
static double investigation_graph_view_get_node_center_y(
    const InvestigationGraphNodeLayout *node_layout
)
{
    return node_layout != NULL
        ? node_layout->y + (node_layout->height / 2.0)
        : 0.0;
}

/**
 * @brief Retourne le texte principal d'une entité.
 *
 * La chaîne retournée est empruntée au modèle.
 */
static const char *investigation_graph_view_get_entity_title(
    const EntityRecord *entity_record
)
{
    const char *label =
        NULL;

    const char *value =
        NULL;

    const char *identifier =
        NULL;

    if (entity_record == NULL)
    {
        return "(entité invalide)";
    }

    label =
        entity_record_get_label(
            entity_record
        );

    if (label != NULL &&
        label[0] != '\0')
    {
        return label;
    }

    value =
        entity_record_get_value(
            entity_record
        );

    if (value != NULL &&
        value[0] != '\0')
    {
        return value;
    }

    identifier =
        entity_record_get_identifier(
            entity_record
        );

    return identifier != NULL &&
           identifier[0] != '\0'
        ? identifier
        : "(sans identifiant)";
}

/**
 * @brief Construit la liste des entités actives.
 *
 * Le tableau retourné appartient à l'appelant. Ses éléments sont empruntés
 * au graphe.
 */
static GPtrArray *investigation_graph_view_list_active_entities(
    const InvestigationGraphModel *graph_model,
    GError **error
)
{
    GPtrArray *all_entities =
        NULL;

    GPtrArray *active_entities =
        NULL;

    guint entity_index =
        0;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (graph_model == NULL)
    {
        return g_ptr_array_new();
    }

    all_entities =
        investigation_graph_model_list_entities(
            graph_model,
            error
        );

    if (all_entities == NULL)
    {
        return NULL;
    }

    active_entities =
        g_ptr_array_new();

    if (active_entities == NULL)
    {
        g_ptr_array_unref(
            all_entities
        );

        g_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer la liste des entités actives."
        );

        return NULL;
    }

    for (entity_index = 0;
         entity_index < all_entities->len;
         entity_index++)
    {
        const EntityRecord *entity_record =
            g_ptr_array_index(
                all_entities,
                entity_index
            );

        if (entity_record_get_status(
                entity_record
            ) != ENTITY_STATUS_ACTIVE)
        {
            continue;
        }

        g_ptr_array_add(
            active_entities,
            (gpointer) entity_record
        );
    }

    g_ptr_array_unref(
        all_entities
    );

    return active_entities;
}

/**
 * @brief Calcule une grille déterministe pour les entités.
 *
 * Le tableau possède les structures InvestigationGraphNodeLayout.
 */
static GPtrArray *investigation_graph_view_create_layout(
    const GPtrArray *active_entities,
    const InvestigationGraphLayout *graph_layout,
    int width,
    gboolean apply_persisted_positions
)
{
    GPtrArray *node_layouts =
        NULL;

    guint column_count =
        1;

    guint entity_index =
        0;

    double available_width =
        0.0;

    double slot_width =
        INVESTIGATION_GRAPH_VIEW_NODE_WIDTH +
        INVESTIGATION_GRAPH_VIEW_HORIZONTAL_GAP;

    if (active_entities == NULL ||
        width <= 0)
    {
        return NULL;
    }

    node_layouts =
        g_ptr_array_new_with_free_func(
            g_free
        );

    if (node_layouts == NULL)
    {
        return NULL;
    }

    available_width =
        (double) width -
        (2.0 * INVESTIGATION_GRAPH_VIEW_MARGIN);

    if (available_width >
        INVESTIGATION_GRAPH_VIEW_NODE_WIDTH)
    {
        column_count =
            (guint) (
                (available_width +
                 INVESTIGATION_GRAPH_VIEW_HORIZONTAL_GAP) /
                slot_width
            );

        if (column_count == 0)
        {
            column_count = 1;
        }
    }

    for (entity_index = 0;
         entity_index < active_entities->len;
         entity_index++)
    {
        InvestigationGraphNodeLayout *node_layout =
            NULL;

        guint column =
            entity_index %
            column_count;

        guint row =
            entity_index /
            column_count;

        node_layout =
            g_try_new0(
                InvestigationGraphNodeLayout,
                1
            );

        if (node_layout == NULL)
        {
            g_ptr_array_unref(
                node_layouts
            );

            return NULL;
        }

        const char *entity_identifier =
            NULL;

        double persisted_x =
            0.0;

        double persisted_y =
            0.0;

        node_layout->kind =
            INVESTIGATION_GRAPH_NODE_KIND_ENTITY;

        node_layout->entity_record =
            g_ptr_array_index(
                active_entities,
                entity_index
            );

        node_layout->relation_record =
            NULL;

        node_layout->width =
            INVESTIGATION_GRAPH_VIEW_NODE_WIDTH;

        node_layout->height =
            INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT;

        node_layout->x =
            INVESTIGATION_GRAPH_VIEW_MARGIN +
            ((double) column * slot_width);

        node_layout->y =
            INVESTIGATION_GRAPH_VIEW_MARGIN +
            ((double) row *
             (INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT +
              INVESTIGATION_GRAPH_VIEW_VERTICAL_GAP));

        entity_identifier =
            entity_record_get_identifier(
                node_layout->entity_record
            );

        if (apply_persisted_positions &&
            graph_layout != NULL &&
            entity_identifier != NULL &&
            entity_identifier[0] != '\0' &&
            investigation_graph_layout_get_position(
                graph_layout,
                entity_identifier,
                &persisted_x,
                &persisted_y
            ))
        {
            node_layout->x =
                persisted_x;

            node_layout->y =
                persisted_y;
        }

        g_ptr_array_add(
            node_layouts,
            node_layout
        );
    }

    return node_layouts;
}

/**
 * @brief Indexe les positions par UUID d'entité.
 *
 * La table et ses clés/valeurs sont empruntées.
 */
static GHashTable *investigation_graph_view_create_layout_index(
    const GPtrArray *node_layouts
)
{
    GHashTable *layout_index =
        NULL;

    guint layout_index_position =
        0;

    if (node_layouts == NULL)
    {
        return NULL;
    }

    layout_index =
        g_hash_table_new(
            g_str_hash,
            g_str_equal
        );

    if (layout_index == NULL)
    {
        return NULL;
    }

    for (layout_index_position = 0;
         layout_index_position < node_layouts->len;
         layout_index_position++)
    {
        InvestigationGraphNodeLayout *node_layout =
            g_ptr_array_index(
                node_layouts,
                layout_index_position
            );

        const char *node_identifier =
            investigation_graph_view_get_node_identifier(
                node_layout
            );

        if (node_identifier == NULL ||
            node_identifier[0] == '\0')
        {
            continue;
        }

        g_hash_table_insert(
            layout_index,
            (gpointer) node_identifier,
            node_layout
        );
    }

    return layout_index;
}

/**
 * @brief Construit une clé déterministe pour un couple source/cible.
 */
static char *investigation_graph_view_create_relation_pair_key(
    const char *source_identifier,
    const char *target_identifier
);

/**
 * @brief Compte les relations actives par couple source/cible.
 */
static GHashTable *investigation_graph_view_count_relation_pairs(
    const GPtrArray *relations,
    GHashTable *layout_index
);

/**
 * @brief Ajoute un nœud graphique pour chaque relation active.
 *
 * Les relations parallèles reçoivent un décalage perpendiculaire afin que
 * leurs nœuds ne soient pas créés exactement au même endroit.
 */
static gboolean investigation_graph_view_append_relation_layouts(
    GPtrArray *node_layouts,
    GHashTable *layout_index,
    const GPtrArray *relations,
    GHashTable *pair_counts,
    const InvestigationGraphLayout *graph_layout,
    gboolean apply_persisted_positions
)
{
    GHashTable *pair_ordinals =
        NULL;

    guint relation_index =
        0;

    (void) graph_layout;
    (void) apply_persisted_positions;

    if (node_layouts == NULL ||
        layout_index == NULL ||
        relations == NULL ||
        pair_counts == NULL)
    {
        return FALSE;
    }

    pair_ordinals =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            NULL
        );

    if (pair_ordinals == NULL)
    {
        return FALSE;
    }

    for (relation_index = 0;
         relation_index < relations->len;
         relation_index++)
    {
        const RelationRecord *relation_record =
            g_ptr_array_index(
                relations,
                relation_index
            );

        const char *relation_identifier =
            NULL;

        const char *source_identifier =
            NULL;

        const char *target_identifier =
            NULL;

        InvestigationGraphNodeLayout *source_layout =
            NULL;

        InvestigationGraphNodeLayout *target_layout =
            NULL;

        InvestigationGraphNodeLayout *relation_layout =
            NULL;

        char *pair_key =
            NULL;

        char *ordinal_key =
            NULL;

        guint pair_count =
            0;

        guint pair_ordinal =
            0;

        double source_center_x =
            0.0;

        double source_center_y =
            0.0;

        double target_center_x =
            0.0;

        double target_center_y =
            0.0;

        double delta_x =
            0.0;

        double delta_y =
            0.0;

        double absolute_delta_x =
            0.0;

        double absolute_delta_y =
            0.0;

        double perpendicular_x =
            0.0;

        double perpendicular_y =
            0.0;

        double parallel_offset =
            0.0;

        if (relation_record == NULL ||
            relation_record_get_status(
                relation_record
            ) != RELATION_STATUS_ACTIVE)
        {
            continue;
        }

        relation_identifier =
            relation_record_get_identifier(
                relation_record
            );

        source_identifier =
            relation_record_get_source_entity_identifier(
                relation_record
            );

        target_identifier =
            relation_record_get_target_entity_identifier(
                relation_record
            );

        if (relation_identifier == NULL ||
            !g_uuid_string_is_valid(
                relation_identifier
            ) ||
            source_identifier == NULL ||
            target_identifier == NULL ||
            g_strcmp0(
                source_identifier,
                target_identifier
            ) == 0)
        {
            continue;
        }

        source_layout =
            g_hash_table_lookup(
                layout_index,
                source_identifier
            );

        target_layout =
            g_hash_table_lookup(
                layout_index,
                target_identifier
            );

        if (source_layout == NULL ||
            target_layout == NULL)
        {
            continue;
        }

        pair_key =
            investigation_graph_view_create_relation_pair_key(
                source_identifier,
                target_identifier
            );

        if (pair_key == NULL)
        {
            g_hash_table_unref(
                pair_ordinals
            );

            return FALSE;
        }

        pair_count =
            GPOINTER_TO_UINT(
                g_hash_table_lookup(
                    pair_counts,
                    pair_key
                )
            );

        pair_ordinal =
            GPOINTER_TO_UINT(
                g_hash_table_lookup(
                    pair_ordinals,
                    pair_key
                )
            );

        ordinal_key =
            g_strdup(
                pair_key
            );

        if (ordinal_key == NULL)
        {
            g_free(
                pair_key
            );

            g_hash_table_unref(
                pair_ordinals
            );

            return FALSE;
        }

        g_hash_table_replace(
            pair_ordinals,
            ordinal_key,
            GUINT_TO_POINTER(
                pair_ordinal + 1U
            )
        );

        relation_layout =
            g_try_new0(
                InvestigationGraphNodeLayout,
                1
            );

        if (relation_layout == NULL)
        {
            g_free(
                pair_key
            );

            g_hash_table_unref(
                pair_ordinals
            );

            return FALSE;
        }

        relation_layout->kind =
            INVESTIGATION_GRAPH_NODE_KIND_RELATION;

        relation_layout->entity_record =
            NULL;

        relation_layout->relation_record =
            relation_record;

        relation_layout->width =
            INVESTIGATION_GRAPH_VIEW_RELATION_NODE_WIDTH;

        relation_layout->height =
            INVESTIGATION_GRAPH_VIEW_RELATION_NODE_HEIGHT;

        source_center_x =
            investigation_graph_view_get_node_center_x(
                source_layout
            );

        source_center_y =
            investigation_graph_view_get_node_center_y(
                source_layout
            );

        target_center_x =
            investigation_graph_view_get_node_center_x(
                target_layout
            );

        target_center_y =
            investigation_graph_view_get_node_center_y(
                target_layout
            );

        delta_x =
            target_center_x -
            source_center_x;

        delta_y =
            target_center_y -
            source_center_y;

        absolute_delta_x =
            delta_x >= 0.0
                ? delta_x
                : -delta_x;

        absolute_delta_y =
            delta_y >= 0.0
                ? delta_y
                : -delta_y;

        /*
         * Un axe perpendiculaire simple suffit pour séparer les relations
         * parallèles sans ajouter de dépendance mathématique au lien final.
         */
        if (absolute_delta_x >=
            absolute_delta_y)
        {
            perpendicular_y =
                1.0;
        }
        else
        {
            perpendicular_x =
                1.0;
        }

        if (pair_count > 1U)
        {
            parallel_offset =
                (
                    (double) pair_ordinal -
                    ((double) (pair_count - 1U) / 2.0)
                ) *
                INVESTIGATION_GRAPH_VIEW_RELATION_NODE_GAP;
        }

        relation_layout->x =
            (
                (source_center_x + target_center_x) /
                2.0
            ) -
            (relation_layout->width / 2.0) +
            (
                perpendicular_x *
                parallel_offset
            );

        relation_layout->relation_offset_x =
            perpendicular_x * parallel_offset;

        relation_layout->y =
            (
                (source_center_y + target_center_y) /
                2.0
            ) -
            (relation_layout->height / 2.0) +
            (
                perpendicular_y *
                parallel_offset
            );

        relation_layout->relation_offset_y =
            perpendicular_y * parallel_offset;

        g_ptr_array_add(
            node_layouts,
            relation_layout
        );

        g_hash_table_insert(
            layout_index,
            (gpointer) relation_identifier,
            relation_layout
        );

        g_free(
            pair_key
        );
    }

    g_hash_table_unref(
        pair_ordinals
    );

    return TRUE;
}

/**
 * @brief Supprime la disposition privée actuellement conservée.
 *
 * Les EntityRecord et RelationRecord contenus dans les collections restent
 * la propriété du graphe métier.
 */
static void investigation_graph_view_clear_layout(
    InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL)
    {
        return;
    }

    investigation_graph_view_cancel_drag(
        graph_view
    );

    investigation_graph_view_set_selected_node(
        graph_view,
        NULL
    );

    graph_view->click_candidate_node =
        NULL;

    graph_view->click_suppressed =
        FALSE;

    g_clear_pointer(
        &graph_view->relation_pair_counts,
        g_hash_table_unref
    );

    g_clear_pointer(
        &graph_view->relations,
        g_ptr_array_unref
    );

    g_clear_pointer(
        &graph_view->node_layouts_by_identifier,
        g_hash_table_unref
    );

    g_clear_pointer(
        &graph_view->node_layouts,
        g_ptr_array_unref
    );

    g_clear_pointer(
        &graph_view->layout_error_message,
        g_free
    );
}

/**
 * @brief Enregistre une erreur de préparation lorsqu'aucune n'existe.
 */
static void investigation_graph_view_set_layout_error(
    GError **error,
    const char *message
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR,
        INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
        message
    );
}

/**
 * @brief Construit et conserve la disposition du graphe courant.
 */
static gboolean investigation_graph_view_build_layout(
    InvestigationGraphView *graph_view,
    int width,
    gboolean apply_persisted_positions,
    GError **error
)
{
    GPtrArray *active_entities =
        NULL;

    GPtrArray *node_layouts =
        NULL;

    GHashTable *node_layouts_by_identifier =
        NULL;

    GPtrArray *relations =
        NULL;

    GHashTable *relation_pair_counts =
        NULL;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (graph_view == NULL ||
        graph_view->graph_model == NULL)
    {
        investigation_graph_view_set_layout_error(
            error,
            "Le graphe à disposer est absent."
        );

        return FALSE;
    }

    if (width <= 0)
    {
        width =
            640;
    }

    active_entities =
        investigation_graph_view_list_active_entities(
            graph_view->graph_model,
            error
        );

    if (active_entities == NULL)
    {
        goto cleanup;
    }

    node_layouts =
        investigation_graph_view_create_layout(
            active_entities,
            graph_view->graph_layout,
            width,
            apply_persisted_positions
        );

    if (node_layouts == NULL)
    {
        investigation_graph_view_set_layout_error(
            error,
            "Impossible d'allouer la disposition des entités."
        );

        goto cleanup;
    }

    node_layouts_by_identifier =
        investigation_graph_view_create_layout_index(
            node_layouts
        );

    if (node_layouts_by_identifier == NULL)
    {
        investigation_graph_view_set_layout_error(
            error,
            "Impossible d'indexer la disposition des entités."
        );

        goto cleanup;
    }

    relations =
        investigation_graph_model_list_relations(
            graph_view->graph_model,
            error
        );

    if (relations == NULL)
    {
        goto cleanup;
    }

    relation_pair_counts =
        investigation_graph_view_count_relation_pairs(
            relations,
            node_layouts_by_identifier
        );

    if (relation_pair_counts == NULL)
    {
        investigation_graph_view_set_layout_error(
            error,
            "Impossible de préparer les relations du graphe."
        );

        goto cleanup;
    }

    if (!investigation_graph_view_append_relation_layouts(
            node_layouts,
            node_layouts_by_identifier,
            relations,
            relation_pair_counts,
            graph_view->graph_layout,
            apply_persisted_positions
        ))
    {
        investigation_graph_view_set_layout_error(
            error,
            "Impossible d'allouer les nœuds de relation."
        );

        goto cleanup;
    }

    graph_view->node_layouts =
        node_layouts;

    graph_view->node_layouts_by_identifier =
        node_layouts_by_identifier;

    graph_view->relations =
        relations;

    graph_view->relation_pair_counts =
        relation_pair_counts;

    node_layouts =
        NULL;

    node_layouts_by_identifier =
        NULL;

    relations =
        NULL;

    relation_pair_counts =
        NULL;

    success =
        TRUE;

cleanup:

    if (relation_pair_counts != NULL)
    {
        g_hash_table_unref(
            relation_pair_counts
        );
    }

    if (relations != NULL)
    {
        g_ptr_array_unref(
            relations
        );
    }

    if (node_layouts_by_identifier != NULL)
    {
        g_hash_table_unref(
            node_layouts_by_identifier
        );
    }

    if (node_layouts != NULL)
    {
        g_ptr_array_unref(
            node_layouts
        );
    }

    if (active_entities != NULL)
    {
        g_ptr_array_unref(
            active_entities
        );
    }

    return success;
}

/**
 * @brief Construit une clé déterministe pour un couple source/cible.
 */
static char *investigation_graph_view_create_relation_pair_key(
    const char *source_identifier,
    const char *target_identifier
)
{
    if (source_identifier == NULL ||
        target_identifier == NULL)
    {
        return NULL;
    }

    return g_strdup_printf(
        "%s|%s",
        source_identifier,
        target_identifier
    );
}

/**
 * @brief Compte les relations actives par couple source/cible.
 */
static GHashTable *investigation_graph_view_count_relation_pairs(
    const GPtrArray *relations,
    GHashTable *layout_index
)
{
    GHashTable *pair_counts =
        NULL;

    guint relation_index =
        0;

    if (relations == NULL ||
        layout_index == NULL)
    {
        return NULL;
    }

    pair_counts =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            NULL
        );

    if (pair_counts == NULL)
    {
        return NULL;
    }

    for (relation_index = 0;
         relation_index < relations->len;
         relation_index++)
    {
        const RelationRecord *relation_record =
            g_ptr_array_index(
                relations,
                relation_index
            );

        const char *source_identifier =
            NULL;

        const char *target_identifier =
            NULL;

        char *pair_key =
            NULL;

        guint relation_count =
            0;

        if (relation_record_get_status(
                relation_record
            ) != RELATION_STATUS_ACTIVE)
        {
            continue;
        }

        source_identifier =
            relation_record_get_source_entity_identifier(
                relation_record
            );

        target_identifier =
            relation_record_get_target_entity_identifier(
                relation_record
            );

        if (source_identifier == NULL ||
            target_identifier == NULL ||
            g_strcmp0(
                source_identifier,
                target_identifier
            ) == 0 ||
            !g_hash_table_contains(
                layout_index,
                source_identifier
            ) ||
            !g_hash_table_contains(
                layout_index,
                target_identifier
            ))
        {
            continue;
        }

        pair_key =
            investigation_graph_view_create_relation_pair_key(
                source_identifier,
                target_identifier
            );

        if (pair_key == NULL)
        {
            g_hash_table_unref(
                pair_counts
            );

            return NULL;
        }

        relation_count =
            GPOINTER_TO_UINT(
                g_hash_table_lookup(
                    pair_counts,
                    pair_key
                )
            );

        g_hash_table_replace(
            pair_counts,
            pair_key,
            GUINT_TO_POINTER(
                relation_count + 1U
            )
        );
    }

    return pair_counts;
}

/**
 * @brief Dessine un rectangle aux coins arrondis.
 */
static void investigation_graph_view_rounded_rectangle(
    cairo_t *cairo_context,
    double x,
    double y,
    double width,
    double height,
    double radius
)
{
    double right =
        x + width;

    double bottom =
        y + height;

    cairo_new_sub_path(
        cairo_context
    );

    cairo_arc(
        cairo_context,
        right - radius,
        y + radius,
        radius,
        -G_PI_2,
        0.0
    );

    cairo_arc(
        cairo_context,
        right - radius,
        bottom - radius,
        radius,
        0.0,
        G_PI_2
    );

    cairo_arc(
        cairo_context,
        x + radius,
        bottom - radius,
        radius,
        G_PI_2,
        G_PI
    );

    cairo_arc(
        cairo_context,
        x + radius,
        y + radius,
        radius,
        G_PI,
        3.0 * G_PI_2
    );

    cairo_close_path(
        cairo_context
    );
}

/**
 * @brief Dessine un texte avec Pango et troncature visuelle.
 */
static void investigation_graph_view_draw_text(
    cairo_t *cairo_context,
    const char *text,
    double x,
    double y,
    int width,
    const char *font_description,
    double red,
    double green,
    double blue
)
{
    PangoLayout *layout =
        NULL;

    PangoFontDescription *font =
        NULL;

    if (text == NULL ||
        font_description == NULL ||
        width <= 0)
    {
        return;
    }

    layout =
        pango_cairo_create_layout(
            cairo_context
        );

    if (layout == NULL)
    {
        return;
    }

    font =
        pango_font_description_from_string(
            font_description
        );

    if (font == NULL)
    {
        g_object_unref(
            layout
        );

        return;
    }

    pango_layout_set_font_description(
        layout,
        font
    );

    pango_layout_set_text(
        layout,
        text,
        -1
    );

    pango_layout_set_width(
        layout,
        width * PANGO_SCALE
    );

    pango_layout_set_single_paragraph_mode(
        layout,
        TRUE
    );

    pango_layout_set_ellipsize(
        layout,
        PANGO_ELLIPSIZE_END
    );

    cairo_set_source_rgb(
        cairo_context,
        red,
        green,
        blue
    );

    cairo_move_to(
        cairo_context,
        x,
        y
    );

    pango_cairo_show_layout(
        cairo_context,
        layout
    );

    pango_font_description_free(
        font
    );

    g_object_unref(
        layout
    );
}

/**
 * @brief Dessine une arête orientée entre deux nœuds.
 */
static void investigation_graph_view_draw_edge(
    cairo_t *cairo_context,
    const InvestigationGraphNodeLayout *source_layout,
    const InvestigationGraphNodeLayout *target_layout
)
{
    double source_center_x =
        0.0;

    double source_center_y =
        0.0;

    double target_center_x =
        0.0;

    double target_center_y =
        0.0;

    double delta_x =
        0.0;

    double delta_y =
        0.0;

    double absolute_delta_x =
        0.0;

    double absolute_delta_y =
        0.0;

    double start_x =
        0.0;

    double start_y =
        0.0;

    double end_x =
        0.0;

    double end_y =
        0.0;

    if (cairo_context == NULL ||
        source_layout == NULL ||
        target_layout == NULL ||
        source_layout == target_layout)
    {
        return;
    }

    source_center_x =
        investigation_graph_view_get_node_center_x(
            source_layout
        );

    source_center_y =
        investigation_graph_view_get_node_center_y(
            source_layout
        );

    target_center_x =
        investigation_graph_view_get_node_center_x(
            target_layout
        );

    target_center_y =
        investigation_graph_view_get_node_center_y(
            target_layout
        );

    delta_x =
        target_center_x -
        source_center_x;

    delta_y =
        target_center_y -
        source_center_y;

    absolute_delta_x =
        delta_x >= 0.0
            ? delta_x
            : -delta_x;

    absolute_delta_y =
        delta_y >= 0.0
            ? delta_y
            : -delta_y;

    cairo_set_source_rgb(
        cairo_context,
        0.48,
        0.54,
        0.66
    );

    cairo_set_line_width(
        cairo_context,
        1.5
    );

    if (absolute_delta_x >=
        absolute_delta_y)
    {
        double direction =
            delta_x >= 0.0
                ? 1.0
                : -1.0;

        start_x =
            source_center_x +
            (
                direction *
                (source_layout->width / 2.0)
            );

        start_y =
            source_center_y;

        end_x =
            target_center_x -
            (
                direction *
                (target_layout->width / 2.0)
            );

        end_y =
            target_center_y;

        cairo_move_to(
            cairo_context,
            start_x,
            start_y
        );

        cairo_line_to(
            cairo_context,
            end_x,
            end_y
        );

        cairo_stroke(
            cairo_context
        );

        cairo_move_to(
            cairo_context,
            end_x,
            end_y
        );

        cairo_line_to(
            cairo_context,
            end_x -
            (
                direction *
                INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH
            ),
            end_y -
            INVESTIGATION_GRAPH_VIEW_ARROW_HALF_WIDTH
        );

        cairo_move_to(
            cairo_context,
            end_x,
            end_y
        );

        cairo_line_to(
            cairo_context,
            end_x -
            (
                direction *
                INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH
            ),
            end_y +
            INVESTIGATION_GRAPH_VIEW_ARROW_HALF_WIDTH
        );

        cairo_stroke(
            cairo_context
        );

        return;
    }

    {
        double direction =
            delta_y >= 0.0
                ? 1.0
                : -1.0;

        start_x =
            source_center_x;

        start_y =
            source_center_y +
            (
                direction *
                (source_layout->height / 2.0)
            );

        end_x =
            target_center_x;

        end_y =
            target_center_y -
            (
                direction *
                (target_layout->height / 2.0)
            );

        cairo_move_to(
            cairo_context,
            start_x,
            start_y
        );

        cairo_line_to(
            cairo_context,
            end_x,
            end_y
        );

        cairo_stroke(
            cairo_context
        );

        cairo_move_to(
            cairo_context,
            end_x,
            end_y
        );

        cairo_line_to(
            cairo_context,
            end_x -
            INVESTIGATION_GRAPH_VIEW_ARROW_HALF_WIDTH,
            end_y -
            (
                direction *
                INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH
            )
        );

        cairo_move_to(
            cairo_context,
            end_x,
            end_y
        );

        cairo_line_to(
            cairo_context,
            end_x +
            INVESTIGATION_GRAPH_VIEW_ARROW_HALF_WIDTH,
            end_y -
            (
                direction *
                INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH
            )
        );

        cairo_stroke(
            cairo_context
        );
    }
}

/**
 * @brief Dessine les deux arêtes de chaque relation active.
 */
static void investigation_graph_view_draw_relations(
    cairo_t *cairo_context,
    const GPtrArray *relations,
    GHashTable *layout_index
)
{
    guint relation_index =
        0;

    if (cairo_context == NULL ||
        relations == NULL ||
        layout_index == NULL)
    {
        return;
    }

    for (relation_index = 0;
         relation_index < relations->len;
         relation_index++)
    {
        const RelationRecord *relation_record =
            g_ptr_array_index(
                relations,
                relation_index
            );

        const char *relation_identifier =
            NULL;

        const char *source_identifier =
            NULL;

        const char *target_identifier =
            NULL;

        InvestigationGraphNodeLayout *source_layout =
            NULL;

        InvestigationGraphNodeLayout *relation_layout =
            NULL;

        InvestigationGraphNodeLayout *target_layout =
            NULL;

        if (relation_record == NULL ||
            relation_record_get_status(
                relation_record
            ) != RELATION_STATUS_ACTIVE)
        {
            continue;
        }

        relation_identifier =
            relation_record_get_identifier(
                relation_record
            );

        source_identifier =
            relation_record_get_source_entity_identifier(
                relation_record
            );

        target_identifier =
            relation_record_get_target_entity_identifier(
                relation_record
            );

        if (relation_identifier == NULL ||
            source_identifier == NULL ||
            target_identifier == NULL)
        {
            continue;
        }

        source_layout =
            g_hash_table_lookup(
                layout_index,
                source_identifier
            );

        relation_layout =
            g_hash_table_lookup(
                layout_index,
                relation_identifier
            );

        target_layout =
            g_hash_table_lookup(
                layout_index,
                target_identifier
            );

        if (source_layout == NULL ||
            relation_layout == NULL ||
            target_layout == NULL ||
            relation_layout->kind !=
                INVESTIGATION_GRAPH_NODE_KIND_RELATION)
        {
            continue;
        }

        investigation_graph_view_draw_edge(
            cairo_context,
            source_layout,
            target_layout
        );
    }
}

/** @brief Configure la couleur de fond propre à une plateforme sociale. */
static void investigation_graph_view_set_social_icon_color(
    cairo_t *cairo_context,
    SocialPlatform platform
)
{
    switch (platform)
    {
        case SOCIAL_PLATFORM_TIKTOK:
            cairo_set_source_rgb(cairo_context, 0.06, 0.06, 0.08);
            break;
        case SOCIAL_PLATFORM_INSTAGRAM:
            cairo_set_source_rgb(cairo_context, 0.76, 0.18, 0.48);
            break;
        case SOCIAL_PLATFORM_FACEBOOK:
            cairo_set_source_rgb(cairo_context, 0.10, 0.36, 0.72);
            break;
        case SOCIAL_PLATFORM_X:
            cairo_set_source_rgb(cairo_context, 0.05, 0.06, 0.08);
            break;
        case SOCIAL_PLATFORM_TELEGRAM:
            cairo_set_source_rgb(cairo_context, 0.12, 0.60, 0.84);
            break;
        case SOCIAL_PLATFORM_OTHER:
        case SOCIAL_PLATFORM_NONE:
            cairo_set_source_rgb(cairo_context, 0.34, 0.39, 0.50);
            break;
    }
}

/** @brief Dessine le symbole blanc propre à une plateforme sociale. */
static void investigation_graph_view_draw_social_symbol(
    cairo_t *cairo_context,
    SocialPlatform platform,
    double center_x,
    double center_y
)
{
    cairo_set_source_rgb(cairo_context, 1.0, 1.0, 1.0);
    cairo_set_line_width(cairo_context, 2.3);
    cairo_set_line_cap(cairo_context, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cairo_context, CAIRO_LINE_JOIN_ROUND);
    switch (platform)
    {
        case SOCIAL_PLATFORM_TIKTOK:
            cairo_arc(cairo_context, center_x - 4.0, center_y + 6.0,
                4.2, 0.0, 2.0 * G_PI);
            cairo_fill(cairo_context);
            cairo_move_to(cairo_context, center_x, center_y + 6.0);
            cairo_line_to(cairo_context, center_x, center_y - 9.0);
            cairo_line_to(cairo_context, center_x + 4.0, center_y - 6.0);
            cairo_line_to(cairo_context, center_x + 8.0, center_y - 5.0);
            cairo_stroke(cairo_context);
            break;
        case SOCIAL_PLATFORM_INSTAGRAM:
            investigation_graph_view_rounded_rectangle(cairo_context,
                center_x - 10.0, center_y - 10.0, 20.0, 20.0, 5.0);
            cairo_stroke(cairo_context);
            cairo_arc(cairo_context, center_x, center_y, 4.7,
                0.0, 2.0 * G_PI);
            cairo_stroke(cairo_context);
            cairo_arc(cairo_context, center_x + 6.0, center_y - 6.0,
                1.4, 0.0, 2.0 * G_PI);
            cairo_fill(cairo_context);
            break;
        case SOCIAL_PLATFORM_FACEBOOK:
            investigation_graph_view_draw_text(cairo_context, "f",
                center_x - 5.0, center_y - 14.0, 14,
                "Sans Bold 21", 1.0, 1.0, 1.0);
            break;
        case SOCIAL_PLATFORM_X:
            cairo_move_to(cairo_context, center_x - 8.0, center_y - 9.0);
            cairo_line_to(cairo_context, center_x + 8.0, center_y + 9.0);
            cairo_move_to(cairo_context, center_x + 8.0, center_y - 9.0);
            cairo_line_to(cairo_context, center_x - 8.0, center_y + 9.0);
            cairo_stroke(cairo_context);
            break;
        case SOCIAL_PLATFORM_TELEGRAM:
            cairo_move_to(cairo_context, center_x - 11.0, center_y - 1.0);
            cairo_line_to(cairo_context, center_x + 11.0, center_y - 9.0);
            cairo_line_to(cairo_context, center_x + 5.0, center_y + 10.0);
            cairo_line_to(cairo_context, center_x, center_y + 3.0);
            cairo_line_to(cairo_context, center_x - 5.0, center_y + 7.0);
            cairo_close_path(cairo_context);
            cairo_fill(cairo_context);
            break;
        case SOCIAL_PLATFORM_OTHER:
            cairo_arc(cairo_context, center_x, center_y - 6.0, 4.0,
                0.0, 2.0 * G_PI);
            cairo_arc(cairo_context, center_x - 8.0, center_y + 7.0, 3.0,
                0.0, 2.0 * G_PI);
            cairo_arc(cairo_context, center_x + 8.0, center_y + 7.0, 3.0,
                0.0, 2.0 * G_PI);
            cairo_fill(cairo_context);
            cairo_move_to(cairo_context, center_x, center_y - 2.0);
            cairo_line_to(cairo_context, center_x - 7.0, center_y + 4.0);
            cairo_move_to(cairo_context, center_x, center_y - 2.0);
            cairo_line_to(cairo_context, center_x + 7.0, center_y + 4.0);
            cairo_stroke(cairo_context);
            break;
        case SOCIAL_PLATFORM_NONE:
            break;
    }
}

/** @brief Dessine le badge vectoriel d'un compte social. */
static void investigation_graph_view_draw_social_icon(
    cairo_t *cairo_context,
    SocialPlatform platform,
    double x,
    double y
)
{
    double center_x = x + (INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_SIZE / 2.0);
    double center_y = y + (INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_SIZE / 2.0);
    investigation_graph_view_set_social_icon_color(cairo_context, platform);
    cairo_arc(cairo_context, center_x, center_y,
        INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_SIZE / 2.0,
        0.0, 2.0 * G_PI);
    cairo_fill(cairo_context);
    investigation_graph_view_draw_social_symbol(
        cairo_context, platform, center_x, center_y);
}

/**
 * @brief Dessine un nœud d'entité.
 */
static void investigation_graph_view_draw_entity(
    cairo_t *cairo_context,
    const EntityRecord *entity_record,
    double x,
    double y,
    gboolean selected
)
{
    const char *title =
        NULL;

    const char *type_identifier =
        NULL;

    const char *type_label =
        NULL;

    SocialPlatform social_platform =
        SOCIAL_PLATFORM_NONE;

    PersonRole person_role = PERSON_ROLE_UNCATEGORIZED;

    double text_x =
        x + INVESTIGATION_GRAPH_VIEW_NODE_PADDING;

    int text_width =
        (int) INVESTIGATION_GRAPH_VIEW_NODE_WIDTH -
        (2 * INVESTIGATION_GRAPH_VIEW_NODE_PADDING);

    if (entity_record == NULL)
    {
        return;
    }

    investigation_graph_view_rounded_rectangle(
        cairo_context,
        x,
        y,
        INVESTIGATION_GRAPH_VIEW_NODE_WIDTH,
        INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT,
        INVESTIGATION_GRAPH_VIEW_NODE_RADIUS
    );

    if (g_strcmp0(entity_record_get_type_identifier(entity_record),
            "person") == 0)
    {
        person_role = entity_record_get_person_role(entity_record);
        switch (person_role)
        {
            case PERSON_ROLE_ALLEGED_SCAMMER:
                cairo_set_source_rgb(cairo_context, 0.55, 0.12, 0.14); break;
            case PERSON_ROLE_VICTIM:
                cairo_set_source_rgb(cairo_context, 0.12, 0.42, 0.22); break;
            case PERSON_ROLE_WITNESS:
                cairo_set_source_rgb(cairo_context, 0.12, 0.30, 0.56); break;
            case PERSON_ROLE_SUSPECT:
                cairo_set_source_rgb(cairo_context, 0.62, 0.32, 0.08); break;
            case PERSON_ROLE_RELATED_PERSON:
                cairo_set_source_rgb(cairo_context, 0.38, 0.20, 0.52); break;
            case PERSON_ROLE_IMPERSONATED_IDENTITY:
                cairo_set_source_rgb(cairo_context, 0.61, 0.35, 0.71); break;
            case PERSON_ROLE_UNCATEGORIZED:
            default:
                cairo_set_source_rgb(cairo_context, 0.25, 0.27, 0.31); break;
        }
    }
    else if (selected)
    {
        cairo_set_source_rgb(
            cairo_context,
            0.20,
            0.23,
            0.30
        );
    }
    else
    {
        cairo_set_source_rgb(
            cairo_context,
            0.16,
            0.18,
            0.22
        );
    }

    cairo_fill_preserve(
        cairo_context
    );

    if (selected)
    {
        cairo_set_source_rgb(
            cairo_context,
            0.56,
            0.72,
            1.00
        );

        cairo_set_line_width(
            cairo_context,
            3.0
        );
    }
    else
    {
        cairo_set_source_rgb(
            cairo_context,
            0.40,
            0.45,
            0.55
        );

        cairo_set_line_width(
            cairo_context,
            1.5
        );
    }

    cairo_stroke(
        cairo_context
    );

    title =
        investigation_graph_view_get_entity_title(
            entity_record
        );

    type_identifier =
        entity_record_get_type_identifier(
            entity_record
        );

    social_platform = social_platform_from_entity_type(type_identifier);

    if (g_strcmp0(type_identifier, "person") == 0)
        type_label = person_role_get_label(person_role);

    if (social_platform != SOCIAL_PLATFORM_NONE)
    {
        investigation_graph_view_draw_social_icon(
            cairo_context,
            social_platform,
            x + INVESTIGATION_GRAPH_VIEW_NODE_PADDING,
            y + 20.0
        );
        text_x += INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_SIZE +
            INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_GAP;
        text_width -= (int) (INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_SIZE +
            INVESTIGATION_GRAPH_VIEW_SOCIAL_ICON_GAP);
        type_label = social_platform_get_label(social_platform);
    }

    investigation_graph_view_draw_text(
        cairo_context,
        title,
        text_x,
        y + 13.0,
        text_width,
        "Sans Bold 11",
        0.95,
        0.96,
        0.98
    );

    investigation_graph_view_draw_text(
        cairo_context,
        type_label != NULL
            ? type_label
            : type_identifier != NULL &&
        type_identifier[0] != '\0'
            ? type_identifier
            : "(type inconnu)",
        text_x,
        y + 44.0,
        text_width,
        "Sans 9",
        0.70,
        0.74,
        0.82
    );
}

/**
 * @brief Retourne le titre principal d'une relation.
 */
static const char *investigation_graph_view_get_relation_title(
    const RelationRecord *relation_record
)
{
    const char *label =
        NULL;

    const char *relation_type =
        NULL;

    if (relation_record == NULL)
    {
        return "(relation invalide)";
    }

    label =
        relation_record_get_label(
            relation_record
        );

    if (label != NULL &&
        label[0] != '\0')
    {
        return label;
    }

    relation_type =
        relation_record_get_relation_type(
            relation_record
        );

    return relation_type != NULL &&
           relation_type[0] != '\0'
        ? relation_type
        : "(relation sans type)";
}

/**
 * @brief Dessine un nœud compact représentant une relation.
 */
static void investigation_graph_view_draw_relation_node(
    cairo_t *cairo_context,
    const RelationRecord *relation_record,
    double x,
    double y,
    gboolean selected
)
{
    const char *title =
        NULL;

    int text_width =
        (int) INVESTIGATION_GRAPH_VIEW_RELATION_NODE_WIDTH -
        (2 * INVESTIGATION_GRAPH_VIEW_NODE_PADDING);

    if (cairo_context == NULL ||
        relation_record == NULL)
    {
        return;
    }

    investigation_graph_view_rounded_rectangle(
        cairo_context,
        x,
        y,
        INVESTIGATION_GRAPH_VIEW_RELATION_NODE_WIDTH,
        INVESTIGATION_GRAPH_VIEW_RELATION_NODE_HEIGHT,
        INVESTIGATION_GRAPH_VIEW_RELATION_NODE_RADIUS
    );

    if (selected)
    {
        cairo_set_source_rgb(
            cairo_context,
            0.22,
            0.25,
            0.32
        );
    }
    else
    {
        cairo_set_source_rgb(
            cairo_context,
            0.12,
            0.14,
            0.18
        );
    }

    cairo_fill_preserve(
        cairo_context
    );

    if (selected)
    {
        cairo_set_source_rgb(
            cairo_context,
            0.56,
            0.72,
            1.00
        );

        cairo_set_line_width(
            cairo_context,
            3.0
        );
    }
    else
    {
        cairo_set_source_rgb(
            cairo_context,
            0.48,
            0.54,
            0.65
        );

        cairo_set_line_width(
            cairo_context,
            1.5
        );
    }

    cairo_stroke(
        cairo_context
    );

    title =
        investigation_graph_view_get_relation_title(
            relation_record
        );

    investigation_graph_view_draw_text(
        cairo_context,
        title,
        x + INVESTIGATION_GRAPH_VIEW_NODE_PADDING,
        y + 6.0,
        text_width,
        "Sans 9",
        0.96,
        0.93,
        0.98
    );

}

/**
 * @brief Dessine un nœud selon sa nature.
 */
static void investigation_graph_view_draw_node(
    cairo_t *cairo_context,
    const InvestigationGraphNodeLayout *node_layout,
    gboolean selected
)
{
    if (node_layout == NULL)
    {
        return;
    }

    if (node_layout->kind ==
        INVESTIGATION_GRAPH_NODE_KIND_ENTITY)
    {
        investigation_graph_view_draw_entity(
            cairo_context,
            node_layout->entity_record,
            node_layout->x,
            node_layout->y,
            selected
        );

        return;
    }

    if (node_layout->kind ==
        INVESTIGATION_GRAPH_NODE_KIND_RELATION)
    {
        investigation_graph_view_draw_relation_node(
            cairo_context,
            node_layout->relation_record,
            node_layout->x,
            node_layout->y,
            selected
        );
    }
}

/**
 * @brief Dessine un message centré dans la vue.
 */
static void investigation_graph_view_draw_message(
    cairo_t *cairo_context,
    const char *message,
    int width,
    int height
)
{
    PangoLayout *layout =
        NULL;

    PangoFontDescription *font =
        NULL;

    int text_width =
        0;

    int text_height =
        0;

    if (message == NULL)
    {
        return;
    }

    layout =
        pango_cairo_create_layout(
            cairo_context
        );

    if (layout == NULL)
    {
        return;
    }

    font =
        pango_font_description_from_string(
            "Sans 11"
        );

    if (font == NULL)
    {
        g_object_unref(
            layout
        );

        return;
    }

    pango_layout_set_font_description(
        layout,
        font
    );

    pango_layout_set_text(
        layout,
        message,
        -1
    );

    pango_layout_get_pixel_size(
        layout,
        &text_width,
        &text_height
    );

    cairo_set_source_rgb(
        cairo_context,
        0.48,
        0.50,
        0.56
    );

    cairo_move_to(
        cairo_context,
        ((double) width - (double) text_width) / 2.0,
        ((double) height - (double) text_height) / 2.0
    );

    pango_cairo_show_layout(
        cairo_context,
        layout
    );

    pango_font_description_free(
        font
    );

    g_object_unref(
        layout
    );
}

/**
 * @brief Dessine le contenu actuel de la vue.
 */
static void investigation_graph_view_draw(
    GtkDrawingArea *drawing_area,
    cairo_t *cairo_context,
    int width,
    int height,
    gpointer user_data
)
{
    InvestigationGraphView *graph_view =
        user_data;

    guint layout_position =
        0;

    (void) drawing_area;

    if (graph_view == NULL ||
        cairo_context == NULL ||
        width <= 0 ||
        height <= 0)
    {
        return;
    }

    cairo_set_source_rgb(
        cairo_context,
        0.08,
        0.09,
        0.11
    );

    cairo_paint(
        cairo_context
    );

    if (graph_view->graph_model == NULL)
    {
        investigation_graph_view_draw_message(
            cairo_context,
            "Aucun graphe à afficher",
            width,
            height
        );

        return;
    }

    if (graph_view->layout_error_message != NULL)
    {
        investigation_graph_view_draw_message(
            cairo_context,
            graph_view->layout_error_message,
            width,
            height
        );

        return;
    }

    if (graph_view->node_layouts == NULL ||
        graph_view->node_layouts_by_identifier == NULL ||
        graph_view->relations == NULL)
    {
        investigation_graph_view_draw_message(
            cairo_context,
            "La disposition du graphe est indisponible",
            width,
            height
        );

        return;
    }

    if (graph_view->node_layouts->len == 0)
    {
        investigation_graph_view_draw_message(
            cairo_context,
            "Aucun nœud actif dans cette enquête",
            width,
            height
        );

        return;
    }

    investigation_graph_view_sync_relation_layouts(graph_view);

    /*
     * Les coordonnées de disposition restent exprimées dans l'espace logique
     * du graphe. Cairo applique ensuite la navigation de la vue.
     */
    cairo_save(
        cairo_context
    );

    cairo_translate(
        cairo_context,
        graph_view->offset_x,
        graph_view->offset_y
    );

    cairo_scale(
        cairo_context,
        graph_view->zoom,
        graph_view->zoom
    );

    /*
     * Les relations sont dessinées en premier afin que les nœuds restent
     * lisibles au premier plan.
     */
    investigation_graph_view_draw_relations(
        cairo_context,
        graph_view->relations,
        graph_view->node_layouts_by_identifier
    );

    for (layout_position = 0;
         layout_position < graph_view->node_layouts->len;
         layout_position++)
    {
        const InvestigationGraphNodeLayout *node_layout =
            g_ptr_array_index(
                graph_view->node_layouts,
                layout_position
            );

        if (node_layout ==
            graph_view->selected_node)
        {
            continue;
        }

        investigation_graph_view_draw_node(
            cairo_context,
            node_layout,
            FALSE
        );
    }

    /*
     * Le nœud sélectionné est dessiné en dernier pour rester au premier plan.
     */
    if (graph_view->selected_node != NULL)
    {
        investigation_graph_view_draw_node(
            cairo_context,
            graph_view->selected_node,
            TRUE
        );
    }

    cairo_restore(
        cairo_context
    );
}

InvestigationGraphView *investigation_graph_view_new(void)
{
    InvestigationGraphView *graph_view =
        NULL;

    graph_view =
        g_try_new0(
            InvestigationGraphView,
            1
        );

    if (graph_view == NULL)
    {
        return NULL;
    }

    graph_view->drawing_area =
        gtk_drawing_area_new();

    if (graph_view->drawing_area == NULL)
    {
        investigation_graph_view_free(
            graph_view
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        graph_view->drawing_area,
        TRUE
    );

    gtk_widget_set_vexpand(
        graph_view->drawing_area,
        TRUE
    );

    graph_view->extraction_drop_target = GTK_DROP_TARGET(
        gtk_drop_target_new(G_TYPE_STRING, GDK_ACTION_COPY));
    g_signal_connect(graph_view->extraction_drop_target, "drop",
        G_CALLBACK(investigation_graph_view_on_extraction_drop), graph_view);
    gtk_widget_add_controller(graph_view->drawing_area,
        GTK_EVENT_CONTROLLER(graph_view->extraction_drop_target));

    gtk_drawing_area_set_content_width(
        GTK_DRAWING_AREA(
            graph_view->drawing_area
        ),
        640
    );

    gtk_drawing_area_set_content_height(
        GTK_DRAWING_AREA(
            graph_view->drawing_area
        ),
        480
    );

    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(
            graph_view->drawing_area
        ),
        investigation_graph_view_draw,
        graph_view,
        NULL
    );

    graph_view->zoom_gesture =
        gtk_gesture_zoom_new();

    graph_view->drag_gesture =
        gtk_gesture_drag_new();

    graph_view->click_gesture =
        gtk_gesture_click_new();

    graph_view->scroll_controller =
        gtk_event_controller_scroll_new(
            GTK_EVENT_CONTROLLER_SCROLL_VERTICAL
        );

    graph_view->motion_controller =
        gtk_event_controller_motion_new();

    if (graph_view->zoom_gesture == NULL ||
        graph_view->drag_gesture == NULL ||
        graph_view->click_gesture == NULL ||
        graph_view->scroll_controller == NULL ||
        graph_view->motion_controller == NULL)
    {
        investigation_graph_view_free(
            graph_view
        );

        return NULL;
    }

    g_signal_connect(
        graph_view->zoom_gesture,
        "begin",
        G_CALLBACK(
            investigation_graph_view_on_zoom_begin
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->zoom_gesture,
        "scale-changed",
        G_CALLBACK(
            investigation_graph_view_on_scale_changed
        ),
        graph_view
    );

    gtk_gesture_single_set_button(
        GTK_GESTURE_SINGLE(
            graph_view->drag_gesture
        ),
        GDK_BUTTON_PRIMARY
    );

    g_signal_connect(
        graph_view->drag_gesture,
        "drag-begin",
        G_CALLBACK(
            investigation_graph_view_on_drag_begin
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->drag_gesture,
        "drag-update",
        G_CALLBACK(
            investigation_graph_view_on_drag_update
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->drag_gesture,
        "drag-end",
        G_CALLBACK(
            investigation_graph_view_on_drag_end
        ),
        graph_view
    );

    gtk_gesture_single_set_button(
        GTK_GESTURE_SINGLE(
            graph_view->click_gesture
        ),
        GDK_BUTTON_PRIMARY
    );

    g_signal_connect(
        graph_view->click_gesture,
        "pressed",
        G_CALLBACK(
            investigation_graph_view_on_click_pressed
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->click_gesture,
        "released",
        G_CALLBACK(
            investigation_graph_view_on_click_released
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->scroll_controller,
        "scroll",
        G_CALLBACK(
            investigation_graph_view_on_scroll
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->motion_controller,
        "enter",
        G_CALLBACK(
            investigation_graph_view_on_pointer_motion
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->motion_controller,
        "motion",
        G_CALLBACK(
            investigation_graph_view_on_pointer_motion
        ),
        graph_view
    );

    g_signal_connect(
        graph_view->motion_controller,
        "leave",
        G_CALLBACK(
            investigation_graph_view_on_pointer_leave
        ),
        graph_view
    );

    gtk_widget_add_controller(
        graph_view->drawing_area,
        GTK_EVENT_CONTROLLER(
            graph_view->zoom_gesture
        )
    );

    gtk_widget_add_controller(
        graph_view->drawing_area,
        GTK_EVENT_CONTROLLER(
            graph_view->drag_gesture
        )
    );

    gtk_widget_add_controller(
        graph_view->drawing_area,
        GTK_EVENT_CONTROLLER(
            graph_view->click_gesture
        )
    );

    /*
     * Les deux gestes doivent partager la même séquence : le glisser peut
     * réclamer l'événement sans empêcher GtkGestureClick de terminer le clic.
     */
    gtk_gesture_group(
        graph_view->drag_gesture,
        graph_view->click_gesture
    );

    gtk_widget_add_controller(
        graph_view->drawing_area,
        graph_view->scroll_controller
    );

    gtk_widget_add_controller(
        graph_view->drawing_area,
        graph_view->motion_controller
    );

    graph_view->zoom =
        1.0;

    graph_view->offset_x =
        0.0;

    graph_view->offset_y =
        0.0;

    graph_view->drag_start_x =
        0.0;

    graph_view->drag_start_y =
        0.0;

    graph_view->drag_origin_offset_x =
        0.0;

    graph_view->drag_origin_offset_y =
        0.0;

    graph_view->dragging =
        FALSE;

    gtk_widget_set_cursor_from_name(
        graph_view->drawing_area,
        "default"
    );

    return graph_view;
}

GtkWidget *investigation_graph_view_get_widget(
    const InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL)
    {
        return NULL;
    }

    return graph_view->drawing_area;
}

/**
 * @brief Reconstruit la disposition privée du graphe affiché.
 *
 * Le zoom et le déplacement global du canvas sont conservés.
 */
static void investigation_graph_view_rebuild_current_layout(
    InvestigationGraphView *graph_view,
    gboolean apply_persisted_positions
)
{
    GError *error =
        NULL;

    int width =
        640;

    if (graph_view == NULL ||
        graph_view->graph_model == NULL)
    {
        return;
    }

    investigation_graph_view_clear_layout(
        graph_view
    );

    if (graph_view->drawing_area != NULL)
    {
        width =
            gtk_widget_get_width(
                graph_view->drawing_area
            );

        if (width <= 0)
        {
            width =
                640;
        }
    }

    if (!investigation_graph_view_build_layout(
            graph_view,
            width,
            apply_persisted_positions,
            &error
        ))
    {
        graph_view->layout_error_message =
            g_strdup(
                error != NULL
                    ? error->message
                    : "Impossible de reconstruire la disposition du graphe."
            );
    }

    g_clear_error(
        &error
    );

    if (graph_view->drawing_area != NULL)
    {
        gtk_widget_queue_draw(
            graph_view->drawing_area
        );
    }
}

void investigation_graph_view_set_graph(
    InvestigationGraphView *graph_view,
    const InvestigationGraphModel *graph_model
)
{
    GError *error =
        NULL;

    int width =
        640;

    if (graph_view == NULL)
    {
        return;
    }

    if (graph_model == NULL)
    {
        investigation_graph_view_clear(
            graph_view
        );

        return;
    }

    investigation_graph_view_clear_layout(
        graph_view
    );

    graph_view->graph_model =
        graph_model;

    if (graph_view->drawing_area != NULL)
    {
        width =
            gtk_widget_get_width(
                graph_view->drawing_area
            );

        if (width <= 0)
        {
            width =
                640;
        }
    }

    if (!investigation_graph_view_build_layout(
            graph_view,
            width,
            TRUE,
            &error
        ))
    {
        graph_view->layout_error_message =
            g_strdup(
                error != NULL
                    ? error->message
                    : "Impossible de préparer la disposition du graphe."
            );
    }

    g_clear_error(
        &error
    );

    investigation_graph_view_reset_view(
        graph_view
    );
}

void investigation_graph_view_set_layout(
    InvestigationGraphView *graph_view,
    const InvestigationGraphLayout *graph_layout
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->graph_layout =
        graph_layout;

    investigation_graph_view_rebuild_current_layout(
        graph_view,
        TRUE
    );
}

void investigation_graph_view_clear(
    InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL)
    {
        return;
    }

    investigation_graph_view_clear_layout(
        graph_view
    );

    graph_view->graph_model =
        NULL;

    graph_view->graph_layout =
        NULL;

    investigation_graph_view_reset_view(
        graph_view
    );
}

void investigation_graph_view_set_selection_callback(
    InvestigationGraphView *graph_view,
    InvestigationGraphViewSelectionCallback callback,
    gpointer user_data
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->selection_callback =
        callback;

    graph_view->selection_user_data =
        user_data;
}

void investigation_graph_view_set_node_moved_callback(
    InvestigationGraphView *graph_view,
    InvestigationGraphViewNodeMovedCallback callback,
    gpointer user_data
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->node_moved_callback =
        callback;

    graph_view->node_moved_user_data =
        user_data;
}

void investigation_graph_view_set_extraction_drop_callback(
    InvestigationGraphView *graph_view,
    InvestigationGraphViewExtractionDropCallback callback,
    gpointer user_data)
{
    if (graph_view == NULL) return;
    graph_view->extraction_drop_callback = callback;
    graph_view->extraction_drop_user_data = user_data;
}

const EntityRecord *investigation_graph_view_get_selected_entity(
    const InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL ||
        graph_view->selected_node == NULL ||
        graph_view->selected_node->kind !=
            INVESTIGATION_GRAPH_NODE_KIND_ENTITY)
    {
        return NULL;
    }

    return graph_view->selected_node->entity_record;
}

gboolean investigation_graph_view_select_entity(
    InvestigationGraphView *graph_view,
    const char *entity_identifier
)
{
    InvestigationGraphNodeLayout *node_layout = NULL;
    int viewport_width = 0;
    int viewport_height = 0;

    if (graph_view == NULL ||
        graph_view->node_layouts_by_identifier == NULL ||
        entity_identifier == NULL ||
        !g_uuid_string_is_valid(entity_identifier))
    {
        return FALSE;
    }

    node_layout = g_hash_table_lookup(
        graph_view->node_layouts_by_identifier,
        entity_identifier
    );

    if (node_layout == NULL ||
        node_layout->kind != INVESTIGATION_GRAPH_NODE_KIND_ENTITY)
    {
        return FALSE;
    }

    investigation_graph_view_set_selected_node(
        graph_view,
        node_layout
    );

    if (graph_view->drawing_area != NULL)
    {
        viewport_width = gtk_widget_get_width(graph_view->drawing_area);
        viewport_height = gtk_widget_get_height(graph_view->drawing_area);

        if (viewport_width > 0 && viewport_height > 0)
        {
            graph_view->offset_x =
                ((double) viewport_width / 2.0) -
                (investigation_graph_view_get_node_center_x(node_layout) *
                 graph_view->zoom);
            graph_view->offset_y =
                ((double) viewport_height / 2.0) -
                (investigation_graph_view_get_node_center_y(node_layout) *
                 graph_view->zoom);
            gtk_widget_queue_draw(graph_view->drawing_area);
        }
    }

    return TRUE;
}

gboolean investigation_graph_view_select_relation(
    InvestigationGraphView *graph_view,
    const char *relation_identifier
)
{
    InvestigationGraphNodeLayout *node_layout = NULL;

    if (graph_view == NULL ||
        graph_view->node_layouts_by_identifier == NULL ||
        relation_identifier == NULL ||
        !g_uuid_string_is_valid(relation_identifier))
    {
        return FALSE;
    }

    node_layout = g_hash_table_lookup(
        graph_view->node_layouts_by_identifier,
        relation_identifier
    );
    if (node_layout == NULL ||
        node_layout->kind != INVESTIGATION_GRAPH_NODE_KIND_RELATION)
    {
        return FALSE;
    }

    investigation_graph_view_set_selected_node(graph_view, node_layout);
    return TRUE;
}

void investigation_graph_view_clear_selection(
    InvestigationGraphView *graph_view
)
{
    investigation_graph_view_set_selected_node(
        graph_view,
        NULL
    );
}

void investigation_graph_view_reset_view(
    InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->zoom =
        1.0;

    graph_view->offset_x =
        0.0;

    graph_view->offset_y =
        0.0;

    graph_view->drag_start_x =
        0.0;

    graph_view->drag_start_y =
        0.0;

    graph_view->drag_origin_offset_x =
        0.0;

    graph_view->drag_origin_offset_y =
        0.0;

    graph_view->zoom_gesture_start_zoom =
        1.0;

    graph_view->zoom_gesture_start_offset_x =
        0.0;

    graph_view->zoom_gesture_start_offset_y =
        0.0;

    graph_view->zoom_gesture_anchor_x =
        0.0;

    graph_view->zoom_gesture_anchor_y =
        0.0;

    investigation_graph_view_cancel_drag(
        graph_view
    );

    if (graph_view->drawing_area != NULL)
    {
        gtk_widget_set_cursor_from_name(
            graph_view->drawing_area,
            "default"
        );

        gtk_widget_queue_draw(
            graph_view->drawing_area
        );
    }
}

gboolean investigation_graph_view_get_view_transform(
    const InvestigationGraphView *graph_view,
    double *zoom,
    double *offset_x,
    double *offset_y)
{
    if (graph_view == NULL || graph_view->graph_model == NULL ||
        zoom == NULL || offset_x == NULL || offset_y == NULL)
        return FALSE;
    *zoom = graph_view->zoom;
    *offset_x = graph_view->offset_x;
    *offset_y = graph_view->offset_y;
    return TRUE;
}

void investigation_graph_view_set_view_transform(
    InvestigationGraphView *graph_view,
    double zoom,
    double offset_x,
    double offset_y)
{
    if (graph_view == NULL ||
        zoom < INVESTIGATION_GRAPH_VIEW_MIN_ZOOM ||
        zoom > INVESTIGATION_GRAPH_VIEW_MAX_ZOOM ||
        !isfinite(zoom) || !isfinite(offset_x) || !isfinite(offset_y))
        return;
    graph_view->zoom = zoom;
    graph_view->offset_x = offset_x;
    graph_view->offset_y = offset_y;
    if (graph_view->drawing_area != NULL)
        gtk_widget_queue_draw(graph_view->drawing_area);
}

void investigation_graph_view_reset_layout(
    InvestigationGraphView *graph_view
)
{
    GError *error =
        NULL;

    int width =
        640;

    if (graph_view == NULL)
    {
        return;
    }

    investigation_graph_view_clear_layout(
        graph_view
    );

    if (graph_view->graph_model != NULL)
    {
        if (graph_view->drawing_area != NULL)
        {
            width =
                gtk_widget_get_width(
                    graph_view->drawing_area
                );

            if (width <= 0)
            {
                width =
                    640;
            }
        }

        if (!investigation_graph_view_build_layout(
                graph_view,
                width,
                FALSE,
                &error
            ))
        {
            graph_view->layout_error_message =
                g_strdup(
                    error != NULL
                        ? error->message
                        : "Impossible de réinitialiser la disposition."
                );
        }
    }

    g_clear_error(
        &error
    );

    if (graph_view->drawing_area != NULL)
    {
        gtk_widget_queue_draw(
            graph_view->drawing_area
        );
    }
}

void investigation_graph_view_free(
    InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->selection_callback =
        NULL;

    graph_view->selection_user_data =
        NULL;

    graph_view->node_moved_callback =
        NULL;

    graph_view->node_moved_user_data =
        NULL;

    investigation_graph_view_clear_layout(
        graph_view
    );

    graph_view->graph_model =
        NULL;

    graph_view->graph_layout =
        NULL;

    if (graph_view->drawing_area != NULL)
    {
        gtk_drawing_area_set_draw_func(
            GTK_DRAWING_AREA(
                graph_view->drawing_area
            ),
            NULL,
            NULL,
            NULL
        );

        if (graph_view->zoom_gesture != NULL)
        {
            g_signal_handlers_disconnect_by_data(
                graph_view->zoom_gesture,
                graph_view
            );

            gtk_widget_remove_controller(
                graph_view->drawing_area,
                GTK_EVENT_CONTROLLER(
                    graph_view->zoom_gesture
                )
            );

            graph_view->zoom_gesture =
                NULL;
        }

        if (graph_view->drag_gesture != NULL)
        {
            g_signal_handlers_disconnect_by_data(
                graph_view->drag_gesture,
                graph_view
            );

            gtk_widget_remove_controller(
                graph_view->drawing_area,
                GTK_EVENT_CONTROLLER(
                    graph_view->drag_gesture
                )
            );

            graph_view->drag_gesture =
                NULL;
        }

        if (graph_view->click_gesture != NULL)
        {
            g_signal_handlers_disconnect_by_data(
                graph_view->click_gesture,
                graph_view
            );

            gtk_widget_remove_controller(
                graph_view->drawing_area,
                GTK_EVENT_CONTROLLER(
                    graph_view->click_gesture
                )
            );

            graph_view->click_gesture =
                NULL;
        }

        if (graph_view->scroll_controller != NULL)
        {
            g_signal_handlers_disconnect_by_data(
                graph_view->scroll_controller,
                graph_view
            );

            gtk_widget_remove_controller(
                graph_view->drawing_area,
                graph_view->scroll_controller
            );

            graph_view->scroll_controller =
                NULL;
        }

        if (graph_view->motion_controller != NULL)
        {
            g_signal_handlers_disconnect_by_data(
                graph_view->motion_controller,
                graph_view
            );

            gtk_widget_remove_controller(
                graph_view->drawing_area,
                graph_view->motion_controller
            );

            graph_view->motion_controller =
                NULL;
        }
    }

    graph_view->drawing_area =
        NULL;

    g_free(
        graph_view
    );
}
