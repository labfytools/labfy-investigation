#include "views/person_ocr_projection_editor.h"
#include <gtk/gtk.h>
#define EVIDENCE "10000000-0000-4000-8000-000000000019"
static GtkWidget *named(GtkWidget *w, const char *n) {
  if (g_strcmp0(gtk_widget_get_name(w), n) == 0)
    return w;
  for (GtkWidget *c = gtk_widget_get_first_child(w); c;
       c = gtk_widget_get_next_sibling(c)) {
    GtkWidget *f = named(c, n);
    if (f)
      return f;
  }
  return NULL;
}
static guint count_named(GtkWidget *w, const char *name) {
  guint count = g_strcmp0(gtk_widget_get_name(w), name) == 0 ? 1 : 0;
  for (GtkWidget *child = gtk_widget_get_first_child(w); child;
       child = gtk_widget_get_next_sibling(child))
    count += count_named(child, name);
  return count;
}
static GtkWidget *label(GtkWidget *w, const char *text) {
  if (GTK_IS_LABEL(w) && g_strcmp0(gtk_label_get_text(GTK_LABEL(w)), text) == 0)
    return w;
  for (GtkWidget *c = gtk_widget_get_first_child(w); c;
       c = gtk_widget_get_next_sibling(c)) {
    GtkWidget *found = label(c, text);
    if (found)
      return found;
  }
  return NULL;
}
static void test_editor(void) {
  PersonOcrProjectionEditor *e = person_ocr_projection_editor_new();
  GtkWidget *root = person_ocr_projection_editor_get_widget(e);
  g_object_ref_sink(root);
  person_ocr_projection_editor_set_runs(e, NULL);
  GtkWidget *empty = label(root,
      "Aucune donnée OCR confirmée ne peut être appliquée à cette personne.\n"
      "La transcription OCR reste enregistrée dans la preuve.");
  GtkWidget *title = label(root,
      "Appliquer des données OCR à la personne — facultatif");
  g_assert_nonnull(empty);
  g_assert_nonnull(title);
  g_assert_true(gtk_widget_get_visible(empty));
  g_assert_false(gtk_widget_get_visible(title));
  GPtrArray *out = NULL;
  GError *error = NULL;
  g_assert_true(person_ocr_projection_editor_collect(e, &out, &error));
  g_assert_cmpuint(out->len, ==, 0);
  g_ptr_array_unref(out);
  IdentityOcrRun *r = identity_ocr_run_new(
      EVIDENCE,
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "identity_card", "front", 1, "fra", "none");
  IdentityFieldObservation *f =
      identity_field_observation_new("surname", "BRUT SPECIMEN", 90, NULL, 0);
  identity_field_observation_accept(f);
  identity_field_observation_set_normalized_value(f, "NORMALISÉ SPECIMEN");
  identity_field_observation_confirm(f, "NOM SPECIMEN");
  identity_ocr_run_add_field(r, f);
  IdentityFieldObservation *partial = identity_field_observation_new(
      "given_names", "BRUT PARTIEL", 72, NULL, 1);
  identity_field_observation_modify(partial, "CORRIGÉ PARTIEL", NULL);
  identity_field_observation_set_value_quality(partial, "partial");
  identity_field_observation_confirm(partial, "PRÉNOM SPECIMEN");
  identity_ocr_run_add_field(r, partial);
  IdentityFieldObservation *rejected = identity_field_observation_new(
      "nationality", "REJETÉ SPECIMEN", 40, NULL, 2);
  identity_field_observation_reject(rejected);
  identity_ocr_run_add_field(r, rejected);
  IdentityFieldObservation *unmapped = identity_field_observation_new(
      "document_number", "DOCUMENT SPECIMEN", 95, NULL, 3);
  identity_field_observation_accept(unmapped);
  identity_field_observation_confirm(unmapped, "DOCUMENT SPECIMEN");
  identity_ocr_run_add_field(r, unmapped);
  GPtrArray *runs = g_ptr_array_new();
  g_ptr_array_add(runs, r);
  person_ocr_projection_editor_set_runs(e, runs);
  g_assert_false(gtk_widget_get_visible(empty));
  g_assert_true(gtk_widget_get_visible(title));
  g_assert_cmpuint(count_named(root, "projection-select"), ==, 2);
  GtkCheckButton *select = GTK_CHECK_BUTTON(named(root, "projection-select"));
  g_assert_false(gtk_check_button_get_active(select));
  gtk_check_button_set_active(select, TRUE);
  g_assert_false(person_ocr_projection_editor_collect(e, &out, &error));
  g_assert_nonnull(error);
  g_assert_true(gtk_check_button_get_active(select));
  g_clear_error(&error);
  GtkDropDown *target = GTK_DROP_DOWN(named(root, "projection-target"));
  GtkDropDown *strategy = GTK_DROP_DOWN(named(root, "projection-strategy"));
  gtk_drop_down_set_selected(target, 1);
  gtk_drop_down_set_selected(strategy, 1);
  g_assert_true(person_ocr_projection_editor_collect(e, &out, &error));
  g_assert_cmpuint(out->len, ==, 1);
  g_ptr_array_unref(out);
  GString *summary = g_string_new(NULL);
  person_ocr_projection_editor_append_summary(e, summary);
  g_assert_nonnull(strstr(summary->str,
      "Valeurs OCR choisies pour projection"));
  g_assert_nonnull(strstr(summary->str, "surname : NOM SPECIMEN"));
  g_string_free(summary, TRUE);
  person_ocr_projection_editor_set_runs(e, NULL);
  g_assert_cmpuint(count_named(root, "projection-select"), ==, 0);
  g_ptr_array_unref(runs);
  identity_ocr_run_free(r);
  g_object_unref(root);
  person_ocr_projection_editor_free(e);
}
int main(int argc, char **argv) {
  gtk_init();
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/projection/editor", test_editor);
  return g_test_run();
}
