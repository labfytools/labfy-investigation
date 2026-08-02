#ifndef LABFY_PERSON_OCR_PROJECTION_EDITOR_H
#define LABFY_PERSON_OCR_PROJECTION_EDITOR_H
#include "models/identity_ocr.h"
#include "models/person_ocr_projection.h"
#include <gtk/gtk.h>
G_BEGIN_DECLS
typedef struct PersonOcrProjectionEditor PersonOcrProjectionEditor;
PersonOcrProjectionEditor *person_ocr_projection_editor_new(void);
GtkWidget *person_ocr_projection_editor_get_widget(PersonOcrProjectionEditor *editor);
void person_ocr_projection_editor_set_runs(PersonOcrProjectionEditor *editor,const GPtrArray *runs);
gboolean person_ocr_projection_editor_collect(PersonOcrProjectionEditor *editor,GPtrArray **projections,GError **error);
void person_ocr_projection_editor_append_summary(
    PersonOcrProjectionEditor *editor, GString *summary);
void person_ocr_projection_editor_free(PersonOcrProjectionEditor *editor);
G_END_DECLS
#endif
