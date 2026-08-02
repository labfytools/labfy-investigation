#include "views/person_ocr_projection_editor.h"
#include "core/person_ocr_projection_mapping.h"
#include "models/identity_traceability.h"
typedef struct {
  GtkCheckButton *selected;
  GtkDropDown *target;
  GtkStringList *targets;
  GtkDropDown *strategy;
  IdentityOcrRun *run;
  IdentityFieldObservation *field;
} Row;
struct PersonOcrProjectionEditor {
  GtkWidget *root;
  GtkLabel *title;
  GtkBox *rows;
  GtkLabel *empty;
  GPtrArray *items;
};
static void row_free(Row *r) {
  if (!r)
    return;
  g_clear_object(&r->targets);
  g_free(r);
}
static const char *status_text(IdentityReviewStatus s) {
  return s == IDENTITY_REVIEW_ACCEPTED   ? "accepted"
         : s == IDENTITY_REVIEW_MODIFIED ? "modified"
         : s == IDENTITY_REVIEW_REJECTED ? "rejected"
         : s == IDENTITY_REVIEW_CONFLICT ? "conflict"
                                         : "proposed";
}
static void clear(PersonOcrProjectionEditor *e) {
  GtkWidget *c = gtk_widget_get_first_child(GTK_WIDGET(e->rows));
  while (c) {
    GtkWidget *n = gtk_widget_get_next_sibling(c);
    gtk_box_remove(e->rows, c);
    c = n;
  }
  g_ptr_array_set_size(e->items, 0);
}
PersonOcrProjectionEditor *person_ocr_projection_editor_new(void) {
  PersonOcrProjectionEditor *e = g_new0(PersonOcrProjectionEditor, 1);
  e->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_name(e->root, "person-ocr-projection-editor");
  e->title = GTK_LABEL(gtk_label_new(
      "Appliquer des données OCR à la personne — facultatif"));
  gtk_box_append(GTK_BOX(e->root), GTK_WIDGET(e->title));
  e->empty = GTK_LABEL(gtk_label_new(
      "Aucune donnée OCR confirmée ne peut être appliquée à cette personne.\n"
      "La transcription OCR reste enregistrée dans la preuve."));
  gtk_label_set_wrap(e->empty, TRUE);
  gtk_box_append(GTK_BOX(e->root), GTK_WIDGET(e->empty));
  e->rows = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 8));
  gtk_box_append(GTK_BOX(e->root), GTK_WIDGET(e->rows));
  e->items = g_ptr_array_new_with_free_func((GDestroyNotify)row_free);
  return e;
}
GtkWidget *
person_ocr_projection_editor_get_widget(PersonOcrProjectionEditor *e) {
  return e ? e->root : NULL;
}
void person_ocr_projection_editor_set_runs(PersonOcrProjectionEditor *e,
                                           const GPtrArray *runs) {
  if (!e)
    return;
  clear(e);
  for (guint i = 0; runs && i < runs->len; i++) {
    IdentityOcrRun *run = g_ptr_array_index((GPtrArray *)runs, i);
    const GPtrArray *fields = identity_ocr_run_get_fields(run);
    for (guint j = 0; fields && j < fields->len; j++) {
      IdentityFieldObservation *f = g_ptr_array_index((GPtrArray *)fields, j);
      const PersonOcrProjectionMapping *m = person_ocr_projection_mapping_for(
          identity_field_observation_get_code(f));
      const char *q = identity_field_observation_get_value_quality(f),
                 *v = identity_field_observation_get_confirmed_value(f);
      const char *s = status_text(identity_field_observation_get_status(f));
      if (!m || !identity_traceability_field_is_projectable(
                    s, q,
                    identity_field_observation_is_human_confirmed(f)
                        ? "human_confirmed"
                        : "unconfirmed",
                    v))
        continue;
      Row *r = g_new0(Row, 1);
      r->run = run;
      r->field = f;
      GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
      char *text = g_strdup_printf(
          "%s%s\nBrute : %s\nNormalisée : %s\nCorrigée : %s\nConfirmée : "
          "%s\nValeur actuelle : —\nRésultat prévisualisé : %s\nQualité : %s — "
          "statut : %s\nPreuve : %s — OcrRun : %s",
          m->label, g_strcmp0(q, "partial") == 0 ? " — Valeur partielle" : "",
          identity_field_observation_get_raw_value(f)
              ? identity_field_observation_get_raw_value(f)
              : "—",
          identity_field_observation_get_normalized_value(f)
              ? identity_field_observation_get_normalized_value(f)
              : "—",
          identity_field_observation_get_corrected_value(f)
              ? identity_field_observation_get_corrected_value(f)
              : "—",
          v, v, q, s, identity_ocr_run_get_evidence_id(run),
          identity_ocr_run_get_identifier(run));
      r->selected = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(text));
      gtk_widget_set_name(GTK_WIDGET(r->selected), "projection-select");
      gtk_box_append(GTK_BOX(box), GTK_WIDGET(r->selected));
      g_free(text);
      r->targets = gtk_string_list_new(NULL);
      gtk_string_list_append(r->targets, "Choisir un champ cible");
      gtk_string_list_append(r->targets, m->label);
      r->target = GTK_DROP_DOWN(
          gtk_drop_down_new(G_LIST_MODEL(g_object_ref(r->targets)), NULL));
      gtk_widget_set_name(GTK_WIDGET(r->target), "projection-target");
      gtk_box_append(GTK_BOX(box), GTK_WIDGET(r->target));
      static const char *strategies[] = {"Conserver la valeur actuelle",
                                         "Renseigner si vide",
                                         "Remplacer explicitement", NULL};
      r->strategy = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(strategies));
      gtk_widget_set_name(GTK_WIDGET(r->strategy), "projection-strategy");
      gtk_box_append(GTK_BOX(box), GTK_WIDGET(r->strategy));
      gtk_box_append(e->rows, box);
      g_ptr_array_add(e->items, r);
    }
  }
  gtk_widget_set_visible(GTK_WIDGET(e->title), e->items->len > 0);
  gtk_widget_set_visible(GTK_WIDGET(e->empty), e->items->len == 0);
}
gboolean person_ocr_projection_editor_collect(PersonOcrProjectionEditor *e,
                                              GPtrArray **out, GError **error) {
  if (out)
    *out = NULL;
  if (!e || !out)
    return FALSE;
  GPtrArray *a = g_ptr_array_new_with_free_func(
      (GDestroyNotify)person_ocr_field_projection_free);
  for (guint i = 0; i < e->items->len; i++) {
    Row *r = g_ptr_array_index(e->items, i);
    if (!gtk_check_button_get_active(r->selected))
      continue;
    guint target = gtk_drop_down_get_selected(r->target),
          strategy = gtk_drop_down_get_selected(r->strategy);
    if (target != 1 || strategy == 0) {
      g_set_error_literal(
          error, g_quark_from_static_string("person-ocr-projection-editor"), 1,
          "Choisissez un champ cible et une stratégie d’application.");
      g_ptr_array_unref(a);
      return FALSE;
    }
    const PersonOcrProjectionMapping *m = person_ocr_projection_mapping_for(
        identity_field_observation_get_code(r->field));
    PersonOcrFieldProjection *p = person_ocr_field_projection_new(
        identity_ocr_run_get_evidence_id(r->run),
        identity_ocr_run_get_identifier(r->run),
        identity_field_observation_get_identifier(r->field),
        identity_field_observation_get_code(r->field),
        identity_field_observation_get_confirmed_value(r->field),
        identity_field_observation_get_value_quality(r->field),
        status_text(identity_field_observation_get_status(r->field)),
        m->person_field, NULL,
        strategy == 1 ? PERSON_OCR_FILL_EMPTY : PERSON_OCR_REPLACE_EXISTING,
        TRUE);
    if (!p) {
      g_set_error_literal(
          error, g_quark_from_static_string("person-ocr-projection-editor"), 2,
          "La projection préparée est invalide.");
      g_ptr_array_unref(a);
      return FALSE;
    }
    g_ptr_array_add(a, p);
  }
  *out = a;
  return TRUE;
}
void person_ocr_projection_editor_append_summary(PersonOcrProjectionEditor *e,
                                                 GString *summary) {
  if (!e || !summary)
    return;
  GPtrArray *items = NULL;
  g_string_append(summary, "\n\nValeurs OCR choisies pour projection");
  if (!person_ocr_projection_editor_collect(e, &items, NULL))
    return;
  if (items->len == 0)
    g_string_append(summary, "\nAucune valeur choisie.");
  for (guint i = 0; i < items->len; i++) {
    PersonOcrFieldProjection *item = g_ptr_array_index(items, i);
    g_string_append_printf(summary, "\n• %s : %s",
        person_ocr_field_projection_get_person_field(item),
        person_ocr_field_projection_get_confirmed_value(item));
  }
  g_ptr_array_unref(items);
}
void person_ocr_projection_editor_free(PersonOcrProjectionEditor *e) {
  if (!e)
    return;
  g_ptr_array_unref(e->items);
  g_free(e);
}
