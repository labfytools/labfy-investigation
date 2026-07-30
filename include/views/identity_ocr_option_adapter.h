#ifndef LABFY_IDENTITY_OCR_OPTION_ADAPTER_H
#define LABFY_IDENTITY_OCR_OPTION_ADAPTER_H
#include <glib.h>
G_BEGIN_DECLS
const char *identity_ocr_option_adapter_language_label(const char *code);
guint identity_ocr_option_adapter_default_language_index(
    const GPtrArray *codes);
G_END_DECLS
#endif
