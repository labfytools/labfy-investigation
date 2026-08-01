#include "views/person_factual_relation_editor.h"
#include <gtk/gtk.h>

static GtkWidget *find_named(GtkWidget *widget,const char *name)
{
    if(g_strcmp0(gtk_widget_get_name(widget),name)==0)return widget;
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child;
        child=gtk_widget_get_next_sibling(child)){
        GtkWidget *found=find_named(child,name);if(found)return found;
    }
    return NULL;
}
static void click_named(GtkWidget *root,const char *name)
{g_signal_emit_by_name(find_named(root,name),"clicked");}
static void test_editor(void)
{
    const char *evidence_a="10000000-0000-4000-8000-000000000001";
    const char *evidence_b="10000000-0000-4000-8000-000000000002";
    GtkStringList *labels=gtk_string_list_new((const char*[]){"Preuve A","Preuve B",NULL});
    GPtrArray *ids=g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(ids,g_strdup(evidence_a));g_ptr_array_add(ids,g_strdup(evidence_b));
    GPtrArray *runs=g_ptr_array_new_with_free_func((GDestroyNotify)identity_ocr_run_free);
    g_ptr_array_add(runs,identity_ocr_run_new(evidence_a,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "identity_card","front",1,"fra","none"));
    g_ptr_array_add(runs,identity_ocr_run_new(evidence_b,
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "passport","identity_page",1,"fra","none"));
    PersonFactualRelationEditor *editor=person_factual_relation_editor_new();
    person_factual_relation_editor_set_available_evidence(editor,labels,ids);
    person_factual_relation_editor_set_available_ocr_runs(editor,runs);
    GtkWidget *root=person_factual_relation_editor_get_widget(editor);
    GtkWindow *window=GTK_WINDOW(gtk_window_new());
    gtk_window_set_child(window,root);gtk_window_present(window);
    GPtrArray *relations=NULL;GError *error=NULL;
    g_assert_true(person_factual_relation_editor_collect_relations(editor,&relations,&error));
    g_assert_cmpuint(relations->len,==,0);g_ptr_array_unref(relations);
    click_named(root,"factual-relation-add");
    g_assert_cmpuint(person_factual_relation_editor_get_count(editor),==,1);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(find_named(root,"factual-relation-type")),1);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(find_named(root,"factual-relation-evidence")),1);
    GtkDropDown *run=GTK_DROP_DOWN(find_named(root,"factual-relation-ocr-run"));
    g_assert_cmpuint(g_list_model_get_n_items(gtk_drop_down_get_model(run)),==,2);
    gtk_drop_down_set_selected(run,1);
    gtk_editable_set_text(GTK_EDITABLE(find_named(root,"factual-relation-note")),"  Note SPECIMEN  ");
    g_assert_true(person_factual_relation_editor_collect_relations(editor,&relations,&error));
    g_assert_cmpuint(relations->len,==,1);
    PersonCreationFactualRelationInput *input=g_ptr_array_index(relations,0);
    g_assert_cmpstr(input->factual_note,==,"Note SPECIMEN");g_ptr_array_unref(relations);
    click_named(root,"factual-relation-remove");
    g_assert_cmpuint(person_factual_relation_editor_get_count(editor),==,0);
    gtk_window_destroy(window);while(g_main_context_iteration(NULL,FALSE));
    person_factual_relation_editor_free(editor);g_ptr_array_unref(runs);
    g_ptr_array_unref(ids);g_object_unref(labels);
}
int main(int argc,char **argv)
{
    if(!gtk_init_check()){g_print("SKIP: aucun affichage GTK disponible.\n");return 0;}
    g_test_init(&argc,&argv,NULL);
    g_test_add_func("/factual-relation-editor/explicit-filter-remove",test_editor);
    return g_test_run();
}
