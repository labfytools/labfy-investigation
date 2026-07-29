#ifndef LABFY_OCR_REGION_GEOMETRY_H
#define LABFY_OCR_REGION_GEOMETRY_H

#include <glib.h>

typedef enum {
    OCR_REGION_FIT_CONTAIN,
    OCR_REGION_FIT_FILL
} OcrRegionFit;

typedef struct {
    double x;
    double y;
    double width;
    double height;
} OcrDisplayRegion;

gboolean ocr_region_geometry_transform(gint source_width, gint source_height,
    gint region_x, gint region_y, gint region_width, gint region_height,
    double display_width, double display_height, OcrRegionFit fit,
    OcrDisplayRegion *out_region);

#endif
