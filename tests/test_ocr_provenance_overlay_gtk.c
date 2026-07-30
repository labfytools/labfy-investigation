#include "widgets/ocr_provenance_overlay.h"
#include <glib/gstdio.h>

int main(int argc, char **argv)
{
    (void) argc; (void) argv;
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    OcrProvenanceOverlay *overlay = ocr_provenance_overlay_new();
    GtkWidget *widget = ocr_provenance_overlay_get_widget(overlay);
    g_object_ref_sink(widget);
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, 800, 500);
    char *path = g_strdup_printf("%s/labfy-provenance-%u.png",
        g_get_tmp_dir(), g_random_int());
    g_assert_cmpint(cairo_surface_write_to_png(surface, path),
        ==, CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(surface);
    char *contents = NULL;
    gsize length = 0;
    g_assert_true(g_file_get_contents(path, &contents, &length, NULL));
    GBytes *bytes = g_bytes_new_take(contents, length);
    ocr_provenance_overlay_set_image(overlay, bytes, 1);
    ocr_provenance_overlay_set_page(overlay, 2, 2);
    IdentitySourceBox box = {.page=2,.x=10,.y=20,.width=80,.height=25,
        .image_width=800,.image_height=500,.available=TRUE};
    IdentityFieldObservation *field = identity_field_observation_new(
        "surname", "SPECIMEN", 88, &box, 0);
    ocr_provenance_overlay_set_field(overlay, field, 2);
    g_assert_true(ocr_provenance_overlay_has_region(overlay));
    ocr_provenance_overlay_zoom_in(overlay);
    g_assert_cmpfloat(ocr_provenance_overlay_get_zoom(overlay), ==, 1.25);
    ocr_provenance_overlay_zoom_out(overlay);
    ocr_provenance_overlay_fit(overlay);
    g_assert_cmpfloat(ocr_provenance_overlay_get_zoom(overlay), ==, 0.0);
    ocr_provenance_overlay_set_page(overlay, 3, 3);
    g_assert_false(ocr_provenance_overlay_has_region(overlay));
    IdentityFieldObservation *without = identity_field_observation_new(
        "given_names", "ALICE", 70, NULL, 1);
    ocr_provenance_overlay_set_field(overlay, without, 4);
    g_assert_false(ocr_provenance_overlay_has_region(overlay));
    ocr_provenance_overlay_set_field(overlay, field, 2);
    g_assert_false(ocr_provenance_overlay_has_region(overlay));
    ocr_provenance_overlay_clear(overlay, 5);
    identity_field_observation_free(field);
    identity_field_observation_free(without);
    ocr_provenance_overlay_free(overlay);
    g_object_unref(widget);
    g_bytes_unref(bytes);
    g_remove(path);
    g_free(path);
    return 0;
}
