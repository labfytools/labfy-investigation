#include "views/document_identity_misuse_editor.h"
#include "core/document_identity_misuse_service.h"
#include "database/database.h"
#include <glib/gstdio.h>
#include <sqlite3.h>
#define EVIDENCE "11000000-0000-4000-8000-000000000020"
#define RUN "31000000-0000-4000-8000-000000000020"
#define AT "2026-08-02T10:00:00Z"
static GtkWidget *named(GtkWidget*w,const char*n){if(g_strcmp0(gtk_widget_get_name(w),n)==0)return w;for(GtkWidget*c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c)){GtkWidget*r=named(c,n);if(r)return r;}return NULL;}
static void test_editor(void)
{
 GError*error=NULL;char*dir=g_dir_make_tmp("labfy-misuse-gtk-XXXXXX",&error);g_assert_no_error(error);char*path=g_build_filename(dir,"Enquete.sqlite",NULL);g_assert_true(database_initialize(path,"SPECIMEN",dir));
 sqlite3*s=NULL;g_assert_cmpint(sqlite3_open(path,&s),==,SQLITE_OK);char*message=NULL;
 const char*sql="INSERT INTO preuves(id,name,relative_path,type_id,size_bytes,sha256,imported_at,updated_at,status,locked,original_name) VALUES('" EVIDENCE "','specimen.png','specimen.png',2,8,'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','" AT "','" AT "','active',0,'specimen.png');INSERT INTO identity_ocr_runs(id,evidence_id,expected_sha256,page_number,document_type,document_side,engine,requested_languages,available_languages,parameters,preprocessing_profile,executed_at,status,text_relative_path,text_sha256,tsv_relative_path,tsv_sha256) VALUES('" RUN "','" EVIDENCE "','aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',1,'identity_card','front','tesseract','fra','fra','SPECIMEN','none','" AT "','success','raw.txt','bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','raw.tsv','cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc');";
 g_assert_cmpint(sqlite3_exec(s,sql,NULL,NULL,&message),==,SQLITE_OK);sqlite3_free(message);sqlite3_close(s);
 Database*d=database_open(path);g_assert_nonnull(d);DocumentIdentityMisuseEditor*x=document_identity_misuse_editor_new(d,EVIDENCE);g_assert_nonnull(x);GtkWidget*root=document_identity_misuse_editor_get_widget(x);g_object_ref_sink(root);
 GtkDropDown*status=GTK_DROP_DOWN(named(root,"identity-misuse-status"));GtkDropDown*run=GTK_DROP_DOWN(named(root,"identity-misuse-ocr-run"));GtkLabel*error_label=GTK_LABEL(named(root,"identity-misuse-error"));GtkWidget*save=named(root,"identity-misuse-save");
 g_assert_cmpuint(g_list_model_get_n_items(gtk_drop_down_get_model(status)),==,3);g_assert_cmpuint(g_list_model_get_n_items(gtk_drop_down_get_model(run)),==,2);
 gtk_drop_down_set_selected(status,1);g_signal_emit_by_name(save,"clicked");g_assert_true(gtk_widget_get_visible(GTK_WIDGET(error_label)));
 GtkTextBuffer*j=gtk_text_view_get_buffer(GTK_TEXT_VIEW(named(root,"identity-misuse-justification")));gtk_text_buffer_set_text(j,"Indices SPECIMEN",-1);gtk_drop_down_set_selected(run,1);g_signal_emit_by_name(save,"clicked");g_assert_false(gtk_widget_get_visible(GTK_WIDGET(error_label)));
 GPtrArray*h=document_identity_misuse_service_history(d,EVIDENCE,&error);g_assert_no_error(error);g_assert_cmpuint(h->len,==,1);DocumentIdentityMisuseAssessment*a=g_ptr_array_index(h,0);g_assert_cmpstr(document_identity_misuse_assessment_get_status(a),==,"presumed");g_assert_cmpstr(document_identity_misuse_assessment_get_origin(a),==,"human");g_ptr_array_unref(h);
 g_object_unref(root);document_identity_misuse_editor_free(x);database_close(d);g_remove(path);g_rmdir(dir);g_free(path);g_free(dir);
}
int main(int argc,char**argv){gtk_init();g_test_init(&argc,&argv,NULL);g_test_add_func("/identity-misuse/editor",test_editor);return g_test_run();}
