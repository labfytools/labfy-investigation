#include "widgets/ocr_provenance_overlay.h"
#include "core/ocr_region_geometry.h"

struct OcrProvenanceOverlay {
    GtkWidget *root;
    GtkWidget *layer;
    GtkScrolledWindow *scroll;
    GtkPicture *picture;
    GtkDrawingArea *drawing;
    GtkLabel *details;
    GdkTexture *texture;
    IdentitySourceBox box;
    gboolean has_box;
    guint64 generation;
    guint page;
    guint zoom_index;
    gboolean fit_mode;
    gint image_width;
    gint image_height;
    GtkLabel *zoom_label;
    GtkButton *zoom_out;
    GtkButton *fit;
    GtkButton *zoom_in;
};

static const double provenance_zoom_factors[] = {
    0.25, 0.50, 0.75, 1.00, 1.25, 1.50, 2.00, 3.00, 4.00
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
    overlay->fit_mode = TRUE;
    overlay->zoom_index = 3;
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    overlay->zoom_out = GTK_BUTTON(gtk_button_new_from_icon_name(
        "zoom-out-symbolic"));
    overlay->fit = GTK_BUTTON(gtk_button_new_from_icon_name(
        "zoom-fit-best-symbolic"));
    overlay->zoom_in = GTK_BUTTON(gtk_button_new_from_icon_name(
        "zoom-in-symbolic"));
    overlay->zoom_label = GTK_LABEL(gtk_label_new("Ajusté"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(overlay->zoom_out), "Zoom arrière");
    gtk_widget_set_tooltip_text(GTK_WIDGET(overlay->fit),
        "Ajuster à la fenêtre");
    gtk_widget_set_tooltip_text(GTK_WIDGET(overlay->zoom_in), "Zoom avant");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(overlay->zoom_out));
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(overlay->fit));
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(overlay->zoom_in));
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(overlay->zoom_label));
    overlay->layer = gtk_overlay_new();
    overlay->picture = GTK_PICTURE(gtk_picture_new());
    gtk_picture_set_content_fit(overlay->picture, GTK_CONTENT_FIT_CONTAIN);
    overlay->drawing = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_drawing_area_set_draw_func(overlay->drawing, draw_region, overlay,
        NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay->layer),
        GTK_WIDGET(overlay->picture));
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay->layer),
        GTK_WIDGET(overlay->drawing));
    overlay->scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_widget_set_size_request(GTK_WIDGET(overlay->scroll), 360, 220);
    gtk_scrolled_window_set_child(overlay->scroll, overlay->layer);
    overlay->details = GTK_LABEL(gtk_label_new("Zone source indisponible"));
    gtk_label_set_wrap(overlay->details, TRUE);
    gtk_box_append(GTK_BOX(overlay->root), toolbar);
    gtk_box_append(GTK_BOX(overlay->root), GTK_WIDGET(overlay->scroll));
    gtk_box_append(GTK_BOX(overlay->root), GTK_WIDGET(overlay->details));
    g_signal_connect_swapped(overlay->zoom_out, "clicked",
        G_CALLBACK(ocr_provenance_overlay_zoom_out), overlay);
    g_signal_connect_swapped(overlay->fit, "clicked",
        G_CALLBACK(ocr_provenance_overlay_fit), overlay);
    g_signal_connect_swapped(overlay->zoom_in, "clicked",
        G_CALLBACK(ocr_provenance_overlay_zoom_in), overlay);
    return overlay;
}

static void provenance_apply_zoom(OcrProvenanceOverlay *overlay)
{
    double factor = provenance_zoom_factors[overlay->zoom_index];
    char *label = overlay->fit_mode ? g_strdup("Ajusté") :
        g_strdup_printf("%.0f %%", factor * 100.0);
    gtk_label_set_text(overlay->zoom_label, label);
    g_free(label);
    gtk_widget_set_sensitive(GTK_WIDGET(overlay->zoom_out),
        overlay->fit_mode || overlay->zoom_index > 0);
    gtk_widget_set_sensitive(GTK_WIDGET(overlay->zoom_in),
        overlay->fit_mode || overlay->zoom_index + 1U <
        G_N_ELEMENTS(provenance_zoom_factors));
    gtk_widget_set_sensitive(GTK_WIDGET(overlay->fit), !overlay->fit_mode);
    if (overlay->fit_mode) {
        gtk_widget_set_size_request(overlay->layer, -1, -1);
        gtk_widget_set_hexpand(overlay->layer, TRUE);
        gtk_widget_set_vexpand(overlay->layer, TRUE);
        gtk_scrolled_window_set_policy(overlay->scroll,
            GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    } else {
        gtk_widget_set_hexpand(overlay->layer, FALSE);
        gtk_widget_set_vexpand(overlay->layer, FALSE);
        gtk_widget_set_size_request(overlay->layer,
            MAX(1, (gint) (overlay->image_width * factor)),
            MAX(1, (gint) (overlay->image_height * factor)));
        gtk_scrolled_window_set_policy(overlay->scroll,
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    }
    gtk_widget_queue_draw(GTK_WIDGET(overlay->drawing));
}

void ocr_provenance_overlay_zoom_in(OcrProvenanceOverlay *overlay)
{
    if (overlay == NULL || overlay->texture == NULL) return;
    if (overlay->fit_mode) {
        overlay->fit_mode = FALSE;
        overlay->zoom_index = 4;
    } else if (overlay->zoom_index + 1U <
            G_N_ELEMENTS(provenance_zoom_factors))
        overlay->zoom_index++;
    provenance_apply_zoom(overlay);
}

void ocr_provenance_overlay_zoom_out(OcrProvenanceOverlay *overlay)
{
    if (overlay == NULL || overlay->texture == NULL) return;
    if (overlay->fit_mode) {
        overlay->fit_mode = FALSE;
        overlay->zoom_index = 2;
    } else if (overlay->zoom_index > 0)
        overlay->zoom_index--;
    provenance_apply_zoom(overlay);
}

void ocr_provenance_overlay_fit(OcrProvenanceOverlay *overlay)
{
    if (overlay == NULL || overlay->texture == NULL) return;
    overlay->fit_mode = TRUE;
    provenance_apply_zoom(overlay);
}

double ocr_provenance_overlay_get_zoom(
    const OcrProvenanceOverlay *overlay)
{
    return overlay != NULL && !overlay->fit_mode
        ? provenance_zoom_factors[overlay->zoom_index] : 0.0;
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
    overlay->image_width = overlay->texture != NULL
        ? gdk_texture_get_width(overlay->texture) : 0;
    overlay->image_height = overlay->texture != NULL
        ? gdk_texture_get_height(overlay->texture) : 0;
    gtk_picture_set_paintable(overlay->picture,
        overlay->texture != NULL ? GDK_PAINTABLE(overlay->texture) : NULL);
    overlay->fit_mode = TRUE;
    overlay->zoom_index = 3;
    provenance_apply_zoom(overlay);
    gtk_widget_queue_draw(GTK_WIDGET(overlay->drawing));
}

void ocr_provenance_overlay_set_field(OcrProvenanceOverlay *overlay,
    const IdentityFieldObservation *field, guint64 generation)
{
    if (overlay == NULL || generation < overlay->generation) return;
    overlay->generation = generation;
    const IdentitySourceBox *box = field != NULL
        ? identity_field_observation_get_box(field) : NULL;
    overlay->has_box = box != NULL && box->available &&
        (overlay->page == 0 || box->page == (gint) overlay->page);
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

void ocr_provenance_overlay_set_page(
    OcrProvenanceOverlay *overlay, guint page, guint64 generation)
{
    if (overlay == NULL || generation < overlay->generation) return;
    overlay->generation = generation;
    overlay->page = page;
    if (overlay->has_box && overlay->box.page != (gint) page) {
        overlay->has_box = FALSE;
        gtk_label_set_text(overlay->details,
            "Zone source indisponible sur cette page");
        gtk_widget_queue_draw(GTK_WIDGET(overlay->drawing));
    }
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
