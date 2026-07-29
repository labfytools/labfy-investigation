#include "widgets/ocr_provenance_overlay.h"
#include "core/ocr_region_geometry.h"

struct OcrProvenanceOverlay {
    GtkWidget *root;
    GtkPicture *picture;
    GtkDrawingArea *drawing;
    GtkLabel *details;
    GdkTexture *texture;
    IdentitySourceBox box;
    gboolean has_box;
    guint64 generation;
};

static void draw_region(GtkDrawingArea *area, cairo_t *cr, int width,
    int height, gpointer data)
{
    OcrProvenanceOverlay *overlay = data;
    OcrDisplayRegion region;
    (void) area;
    if (!overlay->has_box ||
        !ocr_region_geometry_transform(overlay->box.image_width,
            overlay->box.image_height, overlay->box.x, overlay->box.y,
            overlay->box.width, overlay->box.height, width, height,
            OCR_REGION_FIT_CONTAIN, &region))
        return;
    cairo_set_source_rgba(cr, 1.0, 0.15, 0.05, 0.20);
    cairo_rectangle(cr, region.x, region.y, region.width, region.height);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1.0, 0.1, 0.0, 0.95);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
}

OcrProvenanceOverlay *ocr_provenance_overlay_new(void)
{
    OcrProvenanceOverlay *overlay = g_new0(OcrProvenanceOverlay, 1);
    overlay->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *layer = gtk_overlay_new();
    overlay->picture = GTK_PICTURE(gtk_picture_new());
    gtk_picture_set_content_fit(overlay->picture, GTK_CONTENT_FIT_CONTAIN);
    overlay->drawing = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_drawing_area_set_draw_func(overlay->drawing, draw_region, overlay,
        NULL);
    gtk_overlay_set_child(GTK_OVERLAY(layer), GTK_WIDGET(overlay->picture));
    gtk_overlay_add_overlay(GTK_OVERLAY(layer), GTK_WIDGET(overlay->drawing));
    gtk_widget_set_size_request(layer, 360, 220);
    overlay->details = GTK_LABEL(gtk_label_new("Zone source indisponible"));
    gtk_label_set_wrap(overlay->details, TRUE);
    gtk_box_append(GTK_BOX(overlay->root), layer);
    gtk_box_append(GTK_BOX(overlay->root), GTK_WIDGET(overlay->details));
    return overlay;
}

void ocr_provenance_overlay_free(OcrProvenanceOverlay *overlay)
{
    if (overlay == NULL) return;
    g_clear_object(&overlay->texture);
    g_free(overlay);
}

GtkWidget *ocr_provenance_overlay_get_widget(OcrProvenanceOverlay *overlay)
{
    return overlay != NULL ? overlay->root : NULL;
}

void ocr_provenance_overlay_set_image(OcrProvenanceOverlay *overlay,
    GBytes *bytes, guint64 generation)
{
    if (overlay == NULL || generation < overlay->generation) return;
    overlay->generation = generation;
    g_clear_object(&overlay->texture);
    overlay->texture = bytes != NULL ? gdk_texture_new_from_bytes(bytes, NULL)
        : NULL;
    gtk_picture_set_paintable(overlay->picture,
        overlay->texture != NULL ? GDK_PAINTABLE(overlay->texture) : NULL);
    gtk_widget_queue_draw(GTK_WIDGET(overlay->drawing));
}

void ocr_provenance_overlay_set_field(OcrProvenanceOverlay *overlay,
    const IdentityFieldObservation *field, guint64 generation)
{
    if (overlay == NULL || generation < overlay->generation) return;
    overlay->generation = generation;
    const IdentitySourceBox *box = field != NULL
        ? identity_field_observation_get_box(field) : NULL;
    overlay->has_box = box != NULL && box->available;
    if (overlay->has_box) overlay->box = *box;
    char *text = overlay->has_box
        ? g_strdup_printf("%s — confiance %.1f — page %d — "
            "coordonnées %d,%d %dx%d — origine %s",
            identity_field_observation_get_raw_value(field),
            identity_field_observation_get_confidence(field), box->page,
            box->x, box->y, box->width, box->height,
            identity_field_observation_get_origin(field))
        : g_strdup("Zone source indisponible");
    gtk_label_set_text(overlay->details, text);
    g_free(text);
    gtk_widget_queue_draw(GTK_WIDGET(overlay->drawing));
}

void ocr_provenance_overlay_clear(OcrProvenanceOverlay *overlay,
    guint64 generation)
{
    if (overlay == NULL || generation < overlay->generation) return;
    overlay->generation = generation;
    overlay->has_box = FALSE;
    gtk_label_set_text(overlay->details, "Zone source indisponible");
    gtk_picture_set_paintable(overlay->picture, NULL);
    g_clear_object(&overlay->texture);
    gtk_widget_queue_draw(GTK_WIDGET(overlay->drawing));
}

gboolean ocr_provenance_overlay_has_region(
    const OcrProvenanceOverlay *overlay)
{
    return overlay != NULL && overlay->has_box;
}
