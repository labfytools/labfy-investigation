#include "views/document_authenticity_editor.h"
#include "core/document_authenticity_service.h"
#include "database/database.h"
#include <glib/gstdio.h>
#include <sqlite3.h>
#define EVIDENCE "10000000-0000-4000-8000-000000000018"
#define RUN "30000000-0000-4000-8000-000000000018"
#define AT "2026-07-30T10:00:00Z"
static GtkWidget *named(GtkWidget*w,const char*n){if(g_strcmp0(gtk_widget_get_name(w),n)==0)return w;for(GtkWidget*c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c)){GtkWidget*f=named(c,n);if(f)return f;}return NULL;}
static void exec_ok(sqlite3*d,const char*s){char*m=NULL;g_assert_cmpint(sqlite3_exec(d,s,NULL,NULL,&m),==,SQLITE_OK);sqlite3_free(m);}
static void remove_fixture(char*dir,char*path){g_remove(path);g_rmdir(dir);g_free(path);g_free(dir);}
static void test_editor(void)
{
 GError*error=NULL;char*dir=g_dir_make_tmp("labfy-auth-gtk-XXXXXX",&error);g_assert_no_error(error);char*path=g_build_filename(dir,"SPECIMEN.sqlite",NULL);g_assert_true(database_initialize(path,"SPECIMEN",dir));
 sqlite3*sql=NULL;g_assert_cmpint(sqlite3_open(path,&sql),==,SQLITE_OK);exec_ok(sql,"PRAGMA foreign_keys=ON;INSERT INTO preuves(id,name,relative_path,type_id,size_bytes,sha256,imported_at,updated_at,status,locked,original_name) VALUES('" EVIDENCE "','specimen.png','specimen.png',2,8,'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','" AT "','" AT "','active',0,'specimen.png');INSERT INTO identity_ocr_runs(id,evidence_id,expected_sha256,page_number,document_type,document_side,engine,requested_languages,available_languages,parameters,preprocessing_profile,executed_at,status,text_relative_path,text_sha256,tsv_relative_path,tsv_sha256) VALUES('" RUN "','" EVIDENCE "','aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',1,'identity_card','front','tesseract','fra','fra','SPECIMEN','none','" AT "','success','raw.txt','bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','raw.tsv','cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc');");sqlite3_close(sql);
 Database*d=database_open(path);g_assert_nonnull(d);g_assert_true(database_migrate_to_latest(d));DocumentAuthenticityEditor*x=document_authenticity_editor_new(d,EVIDENCE);g_assert_nonnull(x);GtkWidget*root=document_authenticity_editor_get_widget(x);g_object_ref_sink(root);
 GtkDropDown*status=GTK_DROP_DOWN(named(root,"authenticity-status"));GtkDropDown*run=GTK_DROP_DOWN(named(root,"authenticity-ocr-run"));GtkWidget*save=named(root,"authenticity-save");GtkLabel*message=GTK_LABEL(named(root,"authenticity-error"));g_assert_cmpuint(g_list_model_get_n_items(gtk_drop_down_get_model(status)),==,5);g_assert_cmpuint(g_list_model_get_n_items(gtk_drop_down_get_model(run)),==,2);
 GPtrArray*h=document_authenticity_service_history(d,EVIDENCE,&error);g_assert_no_error(error);g_assert_cmpuint(h->len,==,0);g_ptr_array_unref(h);
 gtk_drop_down_set_selected(status,2);g_signal_emit_by_name(save,"clicked");g_assert_true(gtk_widget_get_visible(GTK_WIDGET(message)));g_assert_nonnull(g_strstr_len(gtk_label_get_text(message),-1,"justification"));
 GtkTextBuffer*j=gtk_text_view_get_buffer(GTK_TEXT_VIEW(named(root,"authenticity-justification")));gtk_text_buffer_set_text(j,"Élément factuel SPECIMEN",-1);gtk_drop_down_set_selected(run,1);g_signal_emit_by_name(save,"clicked");g_assert_false(gtk_widget_get_visible(GTK_WIDGET(message)));
 h=document_authenticity_service_history(d,EVIDENCE,&error);g_assert_cmpuint(h->len,==,1);DocumentAuthenticityAssessment*a=g_ptr_array_index(h,0);g_assert_cmpstr(document_authenticity_assessment_get_status(a),==,"suspicious");g_assert_cmpstr(document_authenticity_assessment_get_ocr_run_identifier(a),==,RUN);g_assert_cmpstr(document_authenticity_assessment_get_origin(a),==,"human");g_ptr_array_unref(h);
 g_object_unref(root);document_authenticity_editor_free(x);database_close(d);d=database_open(path);h=document_authenticity_service_history(d,EVIDENCE,&error);g_assert_no_error(error);g_assert_cmpuint(h->len,==,1);g_ptr_array_unref(h);database_close(d);remove_fixture(dir,path);
}
int main(int argc,char**argv){gtk_init();g_test_init(&argc,&argv,NULL);g_test_add_func("/authenticity/editor",test_editor);return g_test_run();}
