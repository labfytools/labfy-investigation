/******************************************************************************
 * @file controlled_vocab_dropdown.c
 * @brief Implémentation du widget GTK4 GtkDropDown pour le vocabulaire contrôlé.
 ******************************************************************************/
#include "widgets/controlled_vocab_dropdown.h"

#define CONTROLLED_VOCAB_CATEGORY_KEY "labfy-vocab-category"

GtkWidget *controlled_vocab_dropdown_new(ControlledVocabCategory category,
                                         const char *default_code)
{
    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(category, &count);
    if (items == NULL || count == 0)
        return NULL;

    const char **labels = g_new0(const char *, count + 1);
    for (size_t i = 0; i < count; i++)
    {
        labels[i] = items[i].label;
    }
    labels[count] = NULL;

    GtkStringList *string_list = gtk_string_list_new(labels);
    g_free(labels);

    GtkWidget *dropdown = gtk_drop_down_new(G_LIST_MODEL(string_list), NULL);
    g_object_set_data(G_OBJECT(dropdown), CONTROLLED_VOCAB_CATEGORY_KEY,
                      GUINT_TO_POINTER(category));

    if (default_code != NULL)
    {
        controlled_vocab_dropdown_set_selected_code(dropdown, default_code);
    }

    return dropdown;
}

const char *controlled_vocab_dropdown_get_selected_code(GtkWidget *dropdown)
{
    if (dropdown == NULL || !GTK_IS_DROP_DOWN(dropdown))
        return NULL;

    guint category_val = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dropdown), CONTROLLED_VOCAB_CATEGORY_KEY));
    ControlledVocabCategory category = (ControlledVocabCategory) category_val;

    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
    if (selected == GTK_INVALID_LIST_POSITION)
        return NULL;

    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(category, &count);
    if (items == NULL || selected >= count)
        return NULL;

    return items[selected].code;
}

gboolean controlled_vocab_dropdown_set_selected_code(GtkWidget *dropdown,
                                                      const char *code)
{
    if (dropdown == NULL || !GTK_IS_DROP_DOWN(dropdown) || code == NULL)
        return FALSE;

    guint category_val = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(dropdown), CONTROLLED_VOCAB_CATEGORY_KEY));
    ControlledVocabCategory category = (ControlledVocabCategory) category_val;

    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(category, &count);
    if (items == NULL)
        return FALSE;

    for (size_t i = 0; i < count; i++)
    {
        if (g_ascii_strcasecmp(items[i].code, code) == 0)
        {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), (guint) i);
            return TRUE;
        }
    }

    return FALSE;
}
