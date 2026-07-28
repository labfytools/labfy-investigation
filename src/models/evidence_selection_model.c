#include "models/evidence_selection_model.h"

struct EvidenceSelectionModel {
    GPtrArray *records;
    char *query;
    char *type_code;
    char *selected_identifier;
};
static gint compare_strings(gconstpointer left, gconstpointer right)
{
    return g_strcmp0(*(char * const *) left, *(char * const *) right);
}

static char *fold(const char *text)
{
    char *normalized, *result;
    if (text == NULL) return g_strdup("");
    normalized = g_utf8_normalize(text, -1, G_NORMALIZE_ALL_COMPOSE);
    if (normalized == NULL) return g_strdup("");
    result = g_utf8_casefold(normalized, -1);
    g_free(normalized);
    return result != NULL ? result : g_strdup("");
}
static gboolean matches(const EvidenceSelectionModel *m,
    const EvidenceRecord *r)
{
    gboolean found = FALSE;
    char *needle, *fields[5];
    if (m->type_code != NULL && g_strcmp0(m->type_code,
            evidence_record_get_type_identifier(r)) != 0) return FALSE;
    needle = fold(m->query);
    if (needle[0] == '\0') found = TRUE;
    fields[0] = fold(evidence_record_get_original_name(r));
    fields[1] = fold(evidence_record_get_description(r));
    fields[2] = fold(evidence_record_get_type_identifier(r));
    fields[3] = fold(evidence_record_get_type_label(r));
    fields[4] = fold(evidence_record_get_mime_type(r));
    for (guint i = 0; !found && i < G_N_ELEMENTS(fields); i++)
        found = strstr(fields[i], needle) != NULL;
    for (guint i = 0; i < G_N_ELEMENTS(fields); i++) g_free(fields[i]);
    g_free(needle);
    return found;
}
EvidenceSelectionModel *evidence_selection_model_new(const GPtrArray *records)
{
    EvidenceSelectionModel *m = g_new0(EvidenceSelectionModel, 1);
    m->records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_record_free);
    for (guint i = 0; records != NULL && i < records->len; i++) {
        EvidenceRecord *copy = evidence_record_copy(
            g_ptr_array_index((GPtrArray *) records, i));
        if (copy != NULL) g_ptr_array_add(m->records, copy);
    }
    return m;
}
void evidence_selection_model_free(EvidenceSelectionModel *m)
{
    if (m == NULL) return;
    g_ptr_array_unref(m->records); g_free(m->query);
    g_free(m->type_code); g_free(m->selected_identifier); g_free(m);
}
static void invalidate_hidden(EvidenceSelectionModel *m)
{
    if (m->selected_identifier == NULL) return;
    for (guint i = 0; i < m->records->len; i++) {
        EvidenceRecord *r = g_ptr_array_index(m->records, i);
        if (g_strcmp0(evidence_record_get_identifier(r),
                m->selected_identifier) == 0 && matches(m, r)) return;
    }
    g_clear_pointer(&m->selected_identifier, g_free);
}
void evidence_selection_model_set_query(EvidenceSelectionModel *m,
    const char *query)
{
    if (m == NULL) return;
    g_free(m->query); m->query = g_strdup(query); invalidate_hidden(m);
}
void evidence_selection_model_set_type(EvidenceSelectionModel *m,
    const char *type_code)
{
    if (m == NULL) return;
    g_free(m->type_code);
    m->type_code = type_code != NULL && type_code[0] != '\0'
        ? g_strdup(type_code) : NULL;
    invalidate_hidden(m);
}
GPtrArray *evidence_selection_model_list_visible(const EvidenceSelectionModel *m)
{
    GPtrArray *result = g_ptr_array_new();
    for (guint i = 0; m != NULL && i < m->records->len; i++) {
        EvidenceRecord *r = g_ptr_array_index(m->records, i);
        if (matches(m, r)) g_ptr_array_add(result, r);
    }
    return result;
}
gboolean evidence_selection_model_select(EvidenceSelectionModel *m,
    const char *identifier)
{
    if (m == NULL) return FALSE;
    g_clear_pointer(&m->selected_identifier, g_free);
    if (identifier == NULL) return TRUE;
    for (guint i = 0; i < m->records->len; i++) {
        EvidenceRecord *r = g_ptr_array_index(m->records, i);
        if (matches(m, r) && g_strcmp0(
                evidence_record_get_identifier(r), identifier) == 0) {
            m->selected_identifier = g_strdup(identifier);
            return TRUE;
        }
    }
    return FALSE;
}
const EvidenceRecord *evidence_selection_model_get_selected(
    const EvidenceSelectionModel *m)
{
    for (guint i = 0; m != NULL && m->selected_identifier != NULL &&
         i < m->records->len; i++) {
        EvidenceRecord *r = g_ptr_array_index(m->records, i);
        if (g_strcmp0(evidence_record_get_identifier(r),
                m->selected_identifier) == 0) return r;
    }
    return NULL;
}
GPtrArray *evidence_selection_model_list_type_codes(
    const EvidenceSelectionModel *m)
{
    GPtrArray *types = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; m != NULL && i < m->records->len; i++) {
        const char *code = evidence_record_get_type_identifier(
            g_ptr_array_index(m->records, i));
        gboolean exists = FALSE;
        for (guint j = 0; code != NULL && j < types->len; j++)
            if (g_strcmp0(code, g_ptr_array_index(types, j)) == 0)
                exists = TRUE;
        if (code != NULL && !exists) g_ptr_array_add(types, g_strdup(code));
    }
    g_ptr_array_sort(types, compare_strings);
    return types;
}
const EvidenceRecord *evidence_selection_model_find_by_sha256(
    const EvidenceSelectionModel *model, const char *sha256)
{
    for (guint i = 0; model != NULL && sha256 != NULL &&
         i < model->records->len; i++) {
        EvidenceRecord *record = g_ptr_array_index(model->records, i);
        if (g_strcmp0(evidence_record_get_sha256(record), sha256) == 0)
            return record;
    }
    return NULL;
}
const EvidenceRecord *evidence_selection_model_find_by_identifier(
    const EvidenceSelectionModel *model, const char *identifier)
{
    for (guint i = 0; model != NULL && identifier != NULL &&
         i < model->records->len; i++) {
        EvidenceRecord *record = g_ptr_array_index(model->records, i);
        if (g_strcmp0(evidence_record_get_identifier(record), identifier) == 0)
            return record;
    }
    return NULL;
}
