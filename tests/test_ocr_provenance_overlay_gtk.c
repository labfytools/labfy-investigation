#include "widgets/ocr_provenance_overlay.h"

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
    IdentitySourceBox box = {.page=2,.x=10,.y=20,.width=80,.height=25,
        .image_width=800,.image_height=500,.available=TRUE};
    IdentityFieldObservation *field = identity_field_observation_new(
        "surname", "SPECIMEN", 88, &box, 0);
    ocr_provenance_overlay_set_field(overlay, field, 2);
    g_assert_true(ocr_provenance_overlay_has_region(overlay));
    IdentityFieldObservation *without = identity_field_observation_new(
        "given_names", "ALICE", 70, NULL, 1);
    ocr_provenance_overlay_set_field(overlay, without, 3);
    g_assert_false(ocr_provenance_overlay_has_region(overlay));
    ocr_provenance_overlay_set_field(overlay, field, 2);
    g_assert_false(ocr_provenance_overlay_has_region(overlay));
    ocr_provenance_overlay_clear(overlay, 4);
    identity_field_observation_free(field);
    identity_field_observation_free(without);
    ocr_provenance_overlay_free(overlay);
    g_object_unref(widget);
    return 0;
}
