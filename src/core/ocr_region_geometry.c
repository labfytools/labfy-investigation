#include "core/ocr_region_geometry.h"

gboolean ocr_region_geometry_transform(gint sw, gint sh, gint x, gint y,
    gint width, gint height, double dw, double dh, OcrRegionFit fit,
    OcrDisplayRegion *out)
{
    if (out == NULL || sw <= 0 || sh <= 0 || width <= 0 || height <= 0 ||
        dw <= 0 || dh <= 0 || fit > OCR_REGION_FIT_FILL)
        return FALSE;
    gint x1 = CLAMP(x, 0, sw);
    gint y1 = CLAMP(y, 0, sh);
    gint x2 = (gint) CLAMP((gint64) x + width, 0, (gint64) sw);
    gint y2 = (gint) CLAMP((gint64) y + height, 0, (gint64) sh);
    if (x2 <= x1 || y2 <= y1) return FALSE;
    double sx = dw / sw, sy = dh / sh, ox = 0, oy = 0;
    if (fit == OCR_REGION_FIT_CONTAIN) {
        sx = sy = MIN(sx, sy);
        ox = (dw - sw * sx) / 2.0;
        oy = (dh - sh * sy) / 2.0;
    }
    out->x = ox + x1 * sx;
    out->y = oy + y1 * sy;
    out->width = (x2 - x1) * sx;
    out->height = (y2 - y1) * sy;
    return TRUE;
}
