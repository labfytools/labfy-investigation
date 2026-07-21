/******************************************************************************
 * @file investigation_graph_view.c
 * @brief Vue GTK en lecture seule du graphe d'enquête.
 ******************************************************************************/

#include "widgets/investigation_graph_view.h"

#include "models/entity_record.h"
#include "models/investigation_graph_model.h"
#include "models/relation_record.h"

#include <glib.h>
#include <pango/pangocairo.h>

#define INVESTIGATION_GRAPH_VIEW_MARGIN 32.0
#define INVESTIGATION_GRAPH_VIEW_NODE_WIDTH 190.0
#define INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT 78.0
#define INVESTIGATION_GRAPH_VIEW_HORIZONTAL_GAP 28.0
#define INVESTIGATION_GRAPH_VIEW_VERTICAL_GAP 28.0
#define INVESTIGATION_GRAPH_VIEW_NODE_RADIUS 12.0
#define INVESTIGATION_GRAPH_VIEW_NODE_PADDING 12
#define INVESTIGATION_GRAPH_VIEW_RELATION_OFFSET 10.0
#define INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH 9.0
#define INVESTIGATION_GRAPH_VIEW_ARROW_HALF_WIDTH 5.0

/**
 * @brief Position privée d'une entité dans la vue.
 */
typedef struct
{
    const EntityRecord *entity_record;

    double x;
    double y;
} InvestigationGraphNodeLayout;

/**
 * @struct InvestigationGraphView
 * @brief État privé de la vue graphique.
 */
struct InvestigationGraphView
{
    GtkWidget *drawing_area;

    const InvestigationGraphModel *graph_model;
};

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
    int width
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

        node_layout->entity_record =
            g_ptr_array_index(
                active_entities,
                entity_index
            );

        node_layout->x =
            INVESTIGATION_GRAPH_VIEW_MARGIN +
            ((double) column * slot_width);

        node_layout->y =
            INVESTIGATION_GRAPH_VIEW_MARGIN +
            ((double) row *
             (INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT +
              INVESTIGATION_GRAPH_VIEW_VERTICAL_GAP));

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

        const char *entity_identifier =
            entity_record_get_identifier(
                node_layout->entity_record
            );

        if (entity_identifier == NULL ||
            entity_identifier[0] == '\0')
        {
            continue;
        }

        g_hash_table_insert(
            layout_index,
            (gpointer) entity_identifier,
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
 * @brief Dessine une relation orientée entre deux nœuds.
 *
 * Les relations parallèles sont décalées perpendiculairement.
 */
static void investigation_graph_view_draw_relation(
    cairo_t *cairo_context,
    const InvestigationGraphNodeLayout *source_layout,
    const InvestigationGraphNodeLayout *target_layout,
    double parallel_offset
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
        source_layout->x +
        (INVESTIGATION_GRAPH_VIEW_NODE_WIDTH / 2.0);

    source_center_y =
        source_layout->y +
        (INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT / 2.0);

    target_center_x =
        target_layout->x +
        (INVESTIGATION_GRAPH_VIEW_NODE_WIDTH / 2.0);

    target_center_y =
        target_layout->y +
        (INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT / 2.0);

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
            (direction *
             (INVESTIGATION_GRAPH_VIEW_NODE_WIDTH / 2.0));

        start_y =
            source_center_y +
            parallel_offset;

        end_x =
            target_center_x -
            (direction *
             (INVESTIGATION_GRAPH_VIEW_NODE_WIDTH / 2.0));

        end_y =
            target_center_y +
            parallel_offset;

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
            (direction *
             INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH),
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
            (direction *
             INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH),
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
            source_center_x +
            parallel_offset;

        start_y =
            source_center_y +
            (direction *
             (INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT / 2.0));

        end_x =
            target_center_x +
            parallel_offset;

        end_y =
            target_center_y -
            (direction *
             (INVESTIGATION_GRAPH_VIEW_NODE_HEIGHT / 2.0));

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
            (direction *
             INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH)
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
            (direction *
             INVESTIGATION_GRAPH_VIEW_ARROW_LENGTH)
        );

        cairo_stroke(
            cairo_context
        );
    }
}

/**
 * @brief Dessine toutes les relations actives valides.
 */
static void investigation_graph_view_draw_relations(
    cairo_t *cairo_context,
    const GPtrArray *relations,
    GHashTable *layout_index,
    GHashTable *pair_counts
)
{
    GHashTable *pair_ordinals =
        NULL;

    guint relation_index =
        0;

    if (cairo_context == NULL ||
        relations == NULL ||
        layout_index == NULL ||
        pair_counts == NULL)
    {
        return;
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

        const char *source_identifier =
            NULL;

        const char *target_identifier =
            NULL;

        InvestigationGraphNodeLayout *source_layout =
            NULL;

        InvestigationGraphNodeLayout *target_layout =
            NULL;

        char *pair_key =
            NULL;

        guint pair_count =
            0;

        guint pair_ordinal =
            0;

        double parallel_offset =
            0.0;

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
            continue;
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

        g_hash_table_replace(
            pair_ordinals,
            g_strdup(
                pair_key
            ),
            GUINT_TO_POINTER(
                pair_ordinal + 1U
            )
        );

        if (pair_count > 1U)
        {
            parallel_offset =
                (
                    (double) pair_ordinal -
                    ((double) (pair_count - 1U) / 2.0)
                ) *
                INVESTIGATION_GRAPH_VIEW_RELATION_OFFSET;
        }

        investigation_graph_view_draw_relation(
            cairo_context,
            source_layout,
            target_layout,
            parallel_offset
        );

        g_free(
            pair_key
        );
    }

    g_hash_table_unref(
        pair_ordinals
    );
}

/**
 * @brief Dessine un nœud d'entité.
 */
static void investigation_graph_view_draw_entity(
    cairo_t *cairo_context,
    const EntityRecord *entity_record,
    double x,
    double y
)
{
    const char *title =
        NULL;

    const char *type_identifier =
        NULL;

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

    cairo_set_source_rgb(
        cairo_context,
        0.16,
        0.18,
        0.22
    );

    cairo_fill_preserve(
        cairo_context
    );

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

    investigation_graph_view_draw_text(
        cairo_context,
        title,
        x + INVESTIGATION_GRAPH_VIEW_NODE_PADDING,
        y + 13.0,
        text_width,
        "Sans Bold 11",
        0.95,
        0.96,
        0.98
    );

    investigation_graph_view_draw_text(
        cairo_context,
        type_identifier != NULL &&
        type_identifier[0] != '\0'
            ? type_identifier
            : "(type inconnu)",
        x + INVESTIGATION_GRAPH_VIEW_NODE_PADDING,
        y + 44.0,
        text_width,
        "Sans 9",
        0.70,
        0.74,
        0.82
    );
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

    GPtrArray *active_entities =
        NULL;

    GPtrArray *node_layouts =
        NULL;

    GPtrArray *relations =
        NULL;

    GHashTable *layout_index =
        NULL;

    GHashTable *pair_counts =
        NULL;

    GError *error =
        NULL;

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

    active_entities =
        investigation_graph_view_list_active_entities(
            graph_view->graph_model,
            &error
        );

    if (active_entities == NULL)
    {
        goto draw_error;
    }

    if (active_entities->len == 0)
    {
        investigation_graph_view_draw_message(
            cairo_context,
            "Aucune entité active dans cette enquête",
            width,
            height
        );

        goto cleanup;
    }

    node_layouts =
        investigation_graph_view_create_layout(
            active_entities,
            width
        );

    layout_index =
        investigation_graph_view_create_layout_index(
            node_layouts
        );

    relations =
        investigation_graph_model_list_relations(
            graph_view->graph_model,
            &error
        );

    if (node_layouts == NULL ||
        layout_index == NULL ||
        relations == NULL)
    {
        goto draw_error;
    }

    pair_counts =
        investigation_graph_view_count_relation_pairs(
            relations,
            layout_index
        );

    if (pair_counts == NULL)
    {
        goto draw_error;
    }

    /*
     * Les relations sont dessinées en premier afin que les nœuds restent
     * lisibles au premier plan.
     */
    investigation_graph_view_draw_relations(
        cairo_context,
        relations,
        layout_index,
        pair_counts
    );

    for (layout_position = 0;
         layout_position < node_layouts->len;
         layout_position++)
    {
        const InvestigationGraphNodeLayout *node_layout =
            g_ptr_array_index(
                node_layouts,
                layout_position
            );

        investigation_graph_view_draw_entity(
            cairo_context,
            node_layout->entity_record,
            node_layout->x,
            node_layout->y
        );
    }

    goto cleanup;

draw_error:

    investigation_graph_view_draw_message(
        cairo_context,
        error != NULL
            ? error->message
            : "Impossible de préparer le rendu du graphe",
        width,
        height
    );

cleanup:

    g_clear_error(
        &error
    );

    if (pair_counts != NULL)
    {
        g_hash_table_unref(
            pair_counts
        );
    }

    if (layout_index != NULL)
    {
        g_hash_table_unref(
            layout_index
        );
    }

    if (relations != NULL)
    {
        g_ptr_array_unref(
            relations
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

void investigation_graph_view_set_graph(
    InvestigationGraphView *graph_view,
    const InvestigationGraphModel *graph_model
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->graph_model =
        graph_model;

    gtk_widget_queue_draw(
        graph_view->drawing_area
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

    graph_view->graph_model =
        NULL;

    gtk_widget_queue_draw(
        graph_view->drawing_area
    );
}

void investigation_graph_view_free(
    InvestigationGraphView *graph_view
)
{
    if (graph_view == NULL)
    {
        return;
    }

    graph_view->graph_model =
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
    }

    graph_view->drawing_area =
        NULL;

    g_free(
        graph_view
    );
}
