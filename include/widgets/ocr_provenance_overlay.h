#ifndef LABFY_OCR_PROVENANCE_OVERLAY_H
#define LABFY_OCR_PROVENANCE_OVERLAY_H

#include "models/identity_ocr.h"
#include <gtk/gtk.h>

typedef struct OcrProvenanceOverlay OcrProvenanceOverlay;

OcrProvenanceOverlay *ocr_provenance_overlay_new(void);
void ocr_provenance_overlay_free(OcrProvenanceOverlay *overlay);
GtkWidget *ocr_provenance_overlay_get_widget(OcrProvenanceOverlay *overlay);
void ocr_provenance_overlay_set_image(OcrProvenanceOverlay *overlay,
    GBytes *png_bytes, guint64 generation);
void ocr_provenance_overlay_set_field(OcrProvenanceOverlay *overlay,
    const IdentityFieldObservation *field, guint64 generation);
void ocr_provenance_overlay_clear(OcrProvenanceOverlay *overlay,
    guint64 generation);
void ocr_provenance_overlay_set_page(
    OcrProvenanceOverlay *overlay, guint page, guint64 generation);
void ocr_provenance_overlay_zoom_in(OcrProvenanceOverlay *overlay);
void ocr_provenance_overlay_zoom_out(OcrProvenanceOverlay *overlay);
void ocr_provenance_overlay_fit(OcrProvenanceOverlay *overlay);
double ocr_provenance_overlay_get_zoom(
    const OcrProvenanceOverlay *overlay);
gboolean ocr_provenance_overlay_has_region(
    const OcrProvenanceOverlay *overlay);

#endif
