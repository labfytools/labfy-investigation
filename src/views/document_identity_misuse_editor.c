#include "views/document_identity_misuse_editor.h"
#include "core/document_identity_misuse_service.h"
#include "dao/identity_ocr_dao.h"
struct DocumentIdentityMisuseEditor{Database*database;char*evidence;GtkWidget*root;
 GtkDropDown*status;GtkStringList*status_labels;GtkDropDown*run;GtkStringList*run_labels;
 GPtrArray*run_ids;GtkTextBuffer*justification;GtkLabel*error;GtkBox*history;};
static void clear_box(GtkBox*b){GtkWidget*c=gtk_widget_get_first_child(GTK_WIDGET(b));while(c){GtkWidget*n=gtk_widget_get_next_sibling(c);gtk_box_remove(b,c);c=n;}}
static char *buffer_text(GtkTextBuffer*b){GtkTextIter s,e;gtk_text_buffer_get_bounds(b,&s,&e);char*t=gtk_text_buffer_get_text(b,&s,&e,FALSE);g_strstrip(t);return t;}
gboolean document_identity_misuse_editor_refresh(DocumentIdentityMisuseEditor*x,GError**error)
{
 if(!x)return FALSE;
 clear_box(x->history);GPtrArray*h=document_identity_misuse_service_history(x->database,x->evidence,error);if(!h)return FALSE;
 if(h->len==0)gtk_box_append(x->history,gtk_label_new("Aucune évaluation humaine enregistrée."));
 for(gint i=(gint)h->len-1;i>=0;i--){DocumentIdentityMisuseAssessment*a=g_ptr_array_index(h,(guint)i);
  const DocumentIdentityMisuseStatus*s=document_identity_misuse_service_status(document_identity_misuse_assessment_get_status(a));
  const char*j=document_identity_misuse_assessment_get_justification(a),*r=document_identity_misuse_assessment_get_ocr_run_identifier(a);
  char*t=g_strdup_printf("%s — %s\nOrigine : humaine%s%s\nJustification : %s",
   s?s->label:"Statut inconnu",document_identity_misuse_assessment_get_assessed_at(a),
   r?" — OcrRun : ":"",r?r:"",j&&*j?j:"Aucune (statut indéterminé)");
  GtkWidget*l=gtk_label_new(t);gtk_label_set_xalign(GTK_LABEL(l),0);gtk_label_set_wrap(GTK_LABEL(l),TRUE);gtk_box_append(x->history,l);g_free(t);}
 g_ptr_array_unref(h);return TRUE;
}
static void save(GtkButton*b,gpointer data)
{
 (void)b;DocumentIdentityMisuseEditor*x=data;guint si=gtk_drop_down_get_selected(x->status),ri=gtk_drop_down_get_selected(x->run);gsize count=0;
 const DocumentIdentityMisuseStatus*s=document_identity_misuse_service_statuses(&count);if(si>=count)return;
 char*j=buffer_text(x->justification);const char*run=ri>0&&ri-1<x->run_ids->len?g_ptr_array_index(x->run_ids,ri-1):NULL;GError*error=NULL;
 if(!document_identity_misuse_service_add(x->database,x->evidence,s[si].code,j,run,NULL,&error)){
  gtk_label_set_text(x->error,error?error->message:"Erreur d’enregistrement.");gtk_widget_set_visible(GTK_WIDGET(x->error),TRUE);g_clear_error(&error);
 }else{gtk_widget_set_visible(GTK_WIDGET(x->error),FALSE);gtk_text_buffer_set_text(x->justification,"",-1);document_identity_misuse_editor_refresh(x,NULL);}g_free(j);
}
DocumentIdentityMisuseEditor *document_identity_misuse_editor_new(Database*d,const char*evidence)
{
 if(!d||!g_uuid_string_is_valid(evidence))return NULL;
 DocumentIdentityMisuseEditor*x=g_new0(DocumentIdentityMisuseEditor,1);x->database=d;x->evidence=g_strdup(evidence);
 x->root=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_widget_set_name(x->root,"document-identity-misuse-editor");
 GtkWidget*title=gtk_label_new("Usage de l’identité du document — évaluation humaine distincte");gtk_label_set_xalign(GTK_LABEL(title),0);gtk_box_append(GTK_BOX(x->root),title);
 x->status_labels=gtk_string_list_new(NULL);gsize count=0;const DocumentIdentityMisuseStatus*s=document_identity_misuse_service_statuses(&count);for(guint i=0;i<count;i++)gtk_string_list_append(x->status_labels,s[i].label);
 x->status=GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(g_object_ref(x->status_labels)),NULL));gtk_widget_set_name(GTK_WIDGET(x->status),"identity-misuse-status");gtk_box_append(GTK_BOX(x->root),GTK_WIDGET(x->status));
 x->run_labels=gtk_string_list_new(NULL);gtk_string_list_append(x->run_labels,"Aucune exécution OCR");x->run_ids=g_ptr_array_new_with_free_func(g_free);GPtrArray*runs=document_identity_misuse_service_list_runs(d,evidence,NULL);
 if(runs){for(guint i=0;i<runs->len;i++){IdentityOcrRunRecord*r=g_ptr_array_index(runs,i);char*l=g_strdup_printf("%s — %s",r->executed_at,r->engine);gtk_string_list_append(x->run_labels,l);g_ptr_array_add(x->run_ids,g_strdup(r->id));g_free(l);}g_ptr_array_unref(runs);}
 x->run=GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(g_object_ref(x->run_labels)),NULL));gtk_widget_set_name(GTK_WIDGET(x->run),"identity-misuse-ocr-run");gtk_box_append(GTK_BOX(x->root),GTK_WIDGET(x->run));
 gtk_box_append(GTK_BOX(x->root),gtk_label_new("Justification factuelle"));GtkWidget*jv=gtk_text_view_new();x->justification=gtk_text_view_get_buffer(GTK_TEXT_VIEW(jv));gtk_widget_set_name(jv,"identity-misuse-justification");gtk_widget_set_size_request(jv,-1,60);gtk_box_append(GTK_BOX(x->root),jv);
 x->error=GTK_LABEL(gtk_label_new(""));gtk_widget_set_name(GTK_WIDGET(x->error),"identity-misuse-error");gtk_widget_add_css_class(GTK_WIDGET(x->error),"error");gtk_widget_set_visible(GTK_WIDGET(x->error),FALSE);gtk_box_append(GTK_BOX(x->root),GTK_WIDGET(x->error));
 GtkWidget*button=gtk_button_new_with_label("Enregistrer l’évaluation humaine");gtk_widget_set_name(button,"identity-misuse-save");g_signal_connect(button,"clicked",G_CALLBACK(save),x);gtk_box_append(GTK_BOX(x->root),button);
 x->history=GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL,4));gtk_widget_set_name(GTK_WIDGET(x->history),"identity-misuse-history");gtk_box_append(GTK_BOX(x->root),GTK_WIDGET(x->history));document_identity_misuse_editor_refresh(x,NULL);return x;
}
GtkWidget *document_identity_misuse_editor_get_widget(DocumentIdentityMisuseEditor*x){return x?x->root:NULL;}
void document_identity_misuse_editor_free(DocumentIdentityMisuseEditor*x){if(!x)return;g_clear_object(&x->status_labels);g_clear_object(&x->run_labels);g_ptr_array_unref(x->run_ids);g_free(x->evidence);g_free(x);}
