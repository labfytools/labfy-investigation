#include "views/identity_ocr_option_adapter.h"

const char *identity_ocr_option_adapter_language_label(const char *code)
{
    if (g_str_equal(code, "fra")) return "Français (fra)";
    if (g_str_equal(code, "eng")) return "Anglais (eng)";
    if (g_str_equal(code, "fra+eng"))
        return "Français + anglais (fra+eng)";
    return code;
}

guint identity_ocr_option_adapter_default_language_index(
    const GPtrArray *codes)
{
    guint english_index = G_MAXUINT;
    for (guint index = 0; codes != NULL && index < codes->len; index++) {
        const char *code = g_ptr_array_index((GPtrArray *) codes, index);
        if (g_strcmp0(code, "fra") == 0) return index;
        if (g_strcmp0(code, "eng") == 0) english_index = index;
    }
    return english_index != G_MAXUINT ? english_index :
        (codes != NULL && codes->len > 0 ? 0 : G_MAXUINT);
}
