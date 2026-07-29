#include "models/person_evidence_selection.h"

struct PersonEvidenceSelectionItem {
    char *identifier;
    PersonEvidenceOrigin origin;
    PersonEvidenceState state;
    EvidenceRecord *record;
    char *evidence_identifier;
    char *source_path;
    char *staging_path;
    char *original_name;
    char *mime_type;
    char *type_identifier;
    char *sha256;
    char *description;
    char *prepared_at;
    guint64 size_bytes;
};
struct PersonEvidenceSelection {
    GPtrArray *items;
    char *active_identifier;
};

static GQuark selection_error(void)
{
    return g_quark_from_static_string("person-evidence-selection-error");
}
static void item_free(gpointer data)
{
    PersonEvidenceSelectionItem *item = data;
    if (item == NULL) return;
    g_free(item->identifier);
    evidence_record_free(item->record);
    g_free(item->evidence_identifier);
    g_free(item->source_path);
    g_free(item->staging_path);
    g_free(item->original_name);
    g_free(item->mime_type);
    g_free(item->type_identifier);
    g_free(item->sha256);
    g_free(item->description);
    g_free(item->prepared_at);
    g_free(item);
}
static PersonEvidenceSelectionItem *find_item(
    const PersonEvidenceSelection *selection, const char *identifier)
{
    for (guint i = 0; selection != NULL && identifier != NULL &&
         i < selection->items->len; i++) {
        PersonEvidenceSelectionItem *item =
            g_ptr_array_index(selection->items, i);
        if (g_strcmp0(item->identifier, identifier) == 0) return item;
    }
    return NULL;
}
static gboolean duplicate(const PersonEvidenceSelection *selection,
    const char *evidence_identifier, const char *source_path,
    const char *sha256)
{
    for (guint i = 0; selection != NULL && i < selection->items->len; i++) {
        PersonEvidenceSelectionItem *item =
            g_ptr_array_index(selection->items, i);
        if ((evidence_identifier != NULL &&
             g_strcmp0(item->evidence_identifier, evidence_identifier) == 0) ||
            (source_path != NULL &&
             g_strcmp0(item->source_path, source_path) == 0) ||
            (sha256 != NULL && g_strcmp0(item->sha256, sha256) == 0))
            return TRUE;
    }
    return FALSE;
}
PersonEvidenceSelection *person_evidence_selection_new(void)
{
    PersonEvidenceSelection *selection =
        g_new0(PersonEvidenceSelection, 1);
    selection->items = g_ptr_array_new_with_free_func(item_free);
    return selection;
}
PersonEvidenceSelection *person_evidence_selection_copy(
    const PersonEvidenceSelection *source)
{
    PersonEvidenceSelection *copy = person_evidence_selection_new();
    for (guint i = 0; source != NULL && i < source->items->len; i++) {
        PersonEvidenceSelectionItem *item =
            g_ptr_array_index(source->items, i);
        if (item->origin == PERSON_EVIDENCE_ORIGIN_EXISTING) {
            if (person_evidence_selection_add_existing(
                    copy, item->record, NULL))
                person_evidence_selection_set_type(copy, item->identifier,
                    item->type_identifier);
        } else {
            person_evidence_selection_add_staged(copy, item->source_path,
                item->staging_path, item->original_name, item->mime_type,
                item->type_identifier, item->size_bytes, item->sha256,
                item->description, item->prepared_at, NULL);
            PersonEvidenceSelectionItem *copied_item =
                g_ptr_array_index(copy->items, copy->items->len - 1);
            g_free(copied_item->identifier);
            copied_item->identifier = g_strdup(item->identifier);
        }
    }
    if (source != NULL && source->active_identifier != NULL) {
        PersonEvidenceSelectionItem *active =
            find_item(source, source->active_identifier);
        for (guint i = 0; active != NULL && i < copy->items->len; i++) {
            PersonEvidenceSelectionItem *candidate =
                g_ptr_array_index(copy->items, i);
            if (g_strcmp0(candidate->evidence_identifier,
                    active->evidence_identifier) == 0 ||
                (candidate->source_path != NULL &&
                 g_strcmp0(candidate->source_path, active->source_path) == 0))
                person_evidence_selection_set_active(
                    copy, candidate->identifier);
        }
    }
    return copy;
}
void person_evidence_selection_free(PersonEvidenceSelection *selection)
{
    if (selection == NULL) return;
    g_ptr_array_unref(selection->items);
    g_free(selection->active_identifier);
    g_free(selection);
}
guint person_evidence_selection_get_count(
    const PersonEvidenceSelection *selection)
{
    return selection != NULL ? selection->items->len : 0;
}
const PersonEvidenceSelectionItem *person_evidence_selection_get(
    const PersonEvidenceSelection *selection, guint index)
{
    return selection != NULL && index < selection->items->len
        ? g_ptr_array_index(selection->items, index) : NULL;
}
const PersonEvidenceSelectionItem *person_evidence_selection_get_active(
    const PersonEvidenceSelection *selection)
{
    return find_item(selection,
        selection != NULL ? selection->active_identifier : NULL);
}
gboolean person_evidence_selection_set_active(
    PersonEvidenceSelection *selection, const char *item_identifier)
{
    if (selection == NULL ||
        (item_identifier != NULL &&
         find_item(selection, item_identifier) == NULL)) return FALSE;
    g_free(selection->active_identifier);
    selection->active_identifier = g_strdup(item_identifier);
    return TRUE;
}
gboolean person_evidence_selection_add_existing(
    PersonEvidenceSelection *selection, const EvidenceRecord *record,
    GError **error)
{
    PersonEvidenceSelectionItem *item;
    const char *identifier;
    if (selection == NULL || record == NULL ||
        (identifier = evidence_record_get_identifier(record)) == NULL) {
        g_set_error_literal(error, selection_error(), 1,
            "La preuve existante est invalide.");
        return FALSE;
    }
    if (duplicate(selection, identifier, NULL,
            evidence_record_get_sha256(record))) {
        g_set_error_literal(error, selection_error(), 2,
            "Cette preuve est déjà dans la sélection.");
        return FALSE;
    }
    item = g_new0(PersonEvidenceSelectionItem, 1);
    item->identifier = g_strdup(identifier);
    item->origin = PERSON_EVIDENCE_ORIGIN_EXISTING;
    item->state = PERSON_EVIDENCE_STATE_READY;
    item->record = evidence_record_copy(record);
    item->evidence_identifier = g_strdup(identifier);
    item->original_name = g_strdup(evidence_record_get_original_name(record));
    item->mime_type = g_strdup(evidence_record_get_mime_type(record));
    item->type_identifier =
        g_strdup(evidence_record_get_type_identifier(record));
    item->sha256 = g_strdup(evidence_record_get_sha256(record));
    item->description = g_strdup(evidence_record_get_description(record));
    item->size_bytes = evidence_record_get_size_bytes(record);
    g_ptr_array_add(selection->items, item);
    if (selection->active_identifier == NULL)
        selection->active_identifier = g_strdup(item->identifier);
    return TRUE;
}
gboolean person_evidence_selection_add_staged(
    PersonEvidenceSelection *selection, const char *source_path,
    const char *staging_path, const char *original_name,
    const char *mime_type, const char *type_identifier, guint64 size_bytes,
    const char *sha256, const char *description, const char *prepared_at,
    GError **error)
{
    PersonEvidenceSelectionItem *item;
    if (selection == NULL || source_path == NULL || staging_path == NULL ||
        original_name == NULL || mime_type == NULL || type_identifier == NULL ||
        sha256 == NULL || prepared_at == NULL) {
        g_set_error_literal(error, selection_error(), 1,
            "Le fichier préparé est invalide.");
        return FALSE;
    }
    if (duplicate(selection, NULL, source_path, sha256)) {
        g_set_error_literal(error, selection_error(), 2,
            "Ce contenu est déjà dans la sélection.");
        return FALSE;
    }
    item = g_new0(PersonEvidenceSelectionItem, 1);
    item->identifier = g_uuid_string_random();
    item->origin = PERSON_EVIDENCE_ORIGIN_STAGED;
    item->state = PERSON_EVIDENCE_STATE_READY;
    item->source_path = g_strdup(source_path);
    item->staging_path = g_strdup(staging_path);
    item->original_name = g_strdup(original_name);
    item->mime_type = g_strdup(mime_type);
    item->type_identifier = g_strdup(type_identifier);
    item->sha256 = g_strdup(sha256);
    item->description = g_strdup(description);
    item->prepared_at = g_strdup(prepared_at);
    item->size_bytes = size_bytes;
    g_ptr_array_add(selection->items, item);
    if (selection->active_identifier == NULL)
        selection->active_identifier = g_strdup(item->identifier);
    return TRUE;
}
gboolean person_evidence_selection_remove(PersonEvidenceSelection *selection,
    const char *item_identifier)
{
    for (guint i = 0; selection != NULL && item_identifier != NULL &&
         i < selection->items->len; i++) {
        PersonEvidenceSelectionItem *item =
            g_ptr_array_index(selection->items, i);
        if (g_strcmp0(item->identifier, item_identifier) == 0) {
            gboolean active = g_strcmp0(selection->active_identifier,
                item_identifier) == 0;
            g_ptr_array_remove_index(selection->items, i);
            if (active) {
                g_clear_pointer(&selection->active_identifier, g_free);
                if (selection->items->len > 0)
                    selection->active_identifier = g_strdup(
                        ((PersonEvidenceSelectionItem *) g_ptr_array_index(
                            selection->items, MIN(i,
                                selection->items->len - 1)))->identifier);
            }
            return TRUE;
        }
    }
    return FALSE;
}
gboolean person_evidence_selection_set_type(PersonEvidenceSelection *selection,
    const char *item_identifier, const char *type_identifier)
{
    PersonEvidenceSelectionItem *item = find_item(selection, item_identifier);
    if (item == NULL || type_identifier == NULL ||
        type_identifier[0] == '\0') return FALSE;
    g_free(item->type_identifier);
    item->type_identifier = g_strdup(type_identifier);
    return TRUE;
}
gboolean person_evidence_selection_is_confirmable(
    const PersonEvidenceSelection *selection)
{
    for (guint i = 0; selection != NULL && i < selection->items->len; i++)
        if (((PersonEvidenceSelectionItem *) g_ptr_array_index(
                selection->items, i))->state != PERSON_EVIDENCE_STATE_READY)
            return FALSE;
    return selection != NULL;
}
const char *person_evidence_selection_find_existing_by_sha256(
    const PersonEvidenceSelection *selection, const char *sha256)
{
    for (guint i = 0; selection != NULL && sha256 != NULL &&
         i < selection->items->len; i++) {
        PersonEvidenceSelectionItem *item =
            g_ptr_array_index(selection->items, i);
        if (item->origin == PERSON_EVIDENCE_ORIGIN_EXISTING &&
            g_strcmp0(item->sha256, sha256) == 0)
            return item->evidence_identifier;
    }
    return NULL;
}
#define ITEM_GETTER(name, type, field) \
type person_evidence_selection_item_get_##name( \
    const PersonEvidenceSelectionItem *item) { \
    return item != NULL ? item->field : (type) 0; \
}
ITEM_GETTER(identifier, const char *, identifier)
ITEM_GETTER(origin, PersonEvidenceOrigin, origin)
ITEM_GETTER(state, PersonEvidenceState, state)
ITEM_GETTER(record, const EvidenceRecord *, record)
ITEM_GETTER(evidence_identifier, const char *, evidence_identifier)
ITEM_GETTER(source_path, const char *, source_path)
ITEM_GETTER(staging_path, const char *, staging_path)
ITEM_GETTER(original_name, const char *, original_name)
ITEM_GETTER(mime_type, const char *, mime_type)
ITEM_GETTER(type_identifier, const char *, type_identifier)
ITEM_GETTER(sha256, const char *, sha256)
ITEM_GETTER(description, const char *, description)
ITEM_GETTER(prepared_at, const char *, prepared_at)
ITEM_GETTER(size_bytes, guint64, size_bytes)
