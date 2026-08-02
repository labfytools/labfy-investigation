#ifndef LABFY_DOCUMENT_IDENTITY_MISUSE_EDITOR_H
#define LABFY_DOCUMENT_IDENTITY_MISUSE_EDITOR_H
#include "database/database.h"
#include <gtk/gtk.h>
G_BEGIN_DECLS
typedef struct DocumentIdentityMisuseEditor DocumentIdentityMisuseEditor;
DocumentIdentityMisuseEditor *document_identity_misuse_editor_new(Database *database,const char *evidence_identifier);
GtkWidget *document_identity_misuse_editor_get_widget(DocumentIdentityMisuseEditor *editor);
gboolean document_identity_misuse_editor_refresh(DocumentIdentityMisuseEditor *editor,GError **error);
void document_identity_misuse_editor_free(DocumentIdentityMisuseEditor *editor);
G_END_DECLS
#endif
