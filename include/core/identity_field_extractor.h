#ifndef LABFY_IDENTITY_FIELD_EXTRACTOR_H
#define LABFY_IDENTITY_FIELD_EXTRACTOR_H
#include "models/identity_ocr.h"
G_BEGIN_DECLS
GPtrArray *identity_field_extractor_extract(const char *text,
    const char *tsv, gint image_width, gint image_height, GError **error);
G_END_DECLS
#endif
