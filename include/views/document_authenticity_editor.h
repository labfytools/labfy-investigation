#ifndef LABFY_DOCUMENT_AUTHENTICITY_EDITOR_H
#define LABFY_DOCUMENT_AUTHENTICITY_EDITOR_H
#include "database/database.h"
#include <gtk/gtk.h>
G_BEGIN_DECLS
typedef struct DocumentAuthenticityEditor DocumentAuthenticityEditor;
DocumentAuthenticityEditor *document_authenticity_editor_new(Database *database,const char *evidence_identifier);
GtkWidget *document_authenticity_editor_get_widget(DocumentAuthenticityEditor *editor);
gboolean document_authenticity_editor_refresh(DocumentAuthenticityEditor *editor,GError **error);
void document_authenticity_editor_free(DocumentAuthenticityEditor *editor);
G_END_DECLS
#endif
