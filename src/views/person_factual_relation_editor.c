#include "views/person_factual_relation_editor.h"
#include "views/person_vocabulary_adapter.h"

typedef struct {
    char *evidence_id, *run_id, *type, *note;
    GtkDropDown *evidence, *run;
} RelationRow;

struct PersonFactualRelationEditor {
    GtkWidget *root, *empty;
    GtkListBox *list;
    GPtrArray *rows, *evidence_ids, *runs;
    GtkStringList *evidence_labels;
};

static GQuark editor_error(void)
{ return g_quark_from_static_string("person-factual-relation-editor"); }
static void row_free(gpointer data)
{
    RelationRow *row=data;
    if(!row)return;
    g_free(row->evidence_id);g_free(row->run_id);g_free(row->type);
    g_free(row->note);g_free(row);
}
static void input_free(gpointer data)
{
    PersonCreationFactualRelationInput *input=data;
    if(!input)return;
    g_free((char*)input->evidence_selection_identifier);
    g_free((char*)input->ocr_run_identifier);
    g_free((char*)input->relation_type);g_free((char*)input->factual_note);
    g_free(input);
}
static void update_empty(PersonFactualRelationEditor *editor)
{
    gboolean empty=editor->rows->len==0;
    gtk_widget_set_visible(editor->empty,empty);
    gtk_widget_set_visible(GTK_WIDGET(editor->list),!empty);
}
static GtkStringList *evidence_model(PersonFactualRelationEditor *editor)
{
    GtkStringList *model=gtk_string_list_new(NULL);
    gtk_string_list_append(model,"Sélectionner une preuve…");
    for(guint i=0;editor->evidence_labels&&
        i<g_list_model_get_n_items(G_LIST_MODEL(editor->evidence_labels));i++)
        gtk_string_list_append(model,
            gtk_string_list_get_string(editor->evidence_labels,i));
    return model;
}
static GtkStringList *run_model(PersonFactualRelationEditor *editor,
    const char *evidence_id)
{
    GtkStringList *model=gtk_string_list_new(NULL);
    gtk_string_list_append(model,"Aucun OcrRun (facultatif)");
    for(guint i=0;editor->runs&&i<editor->runs->len;i++){
        IdentityOcrRun *run=g_ptr_array_index(editor->runs,i);
        if(g_strcmp0(identity_ocr_run_get_evidence_id(run),evidence_id)!=0)
            continue;
        char *label=g_strdup_printf("OcrRun %s",
            identity_ocr_run_get_identifier(run));
        gtk_string_list_append(model,label);g_free(label);
    }
    return model;
}
static const char *filtered_run_id(PersonFactualRelationEditor *editor,
    const char *evidence_id,guint selected)
{
    if(selected==0)return NULL;
    guint position=1;
    for(guint i=0;editor->runs&&i<editor->runs->len;i++){
        IdentityOcrRun *run=g_ptr_array_index(editor->runs,i);
        if(g_strcmp0(identity_ocr_run_get_evidence_id(run),evidence_id)!=0)
            continue;
        if(position++==selected)return identity_ocr_run_get_identifier(run);
    }
    return NULL;
}
static void evidence_changed(GtkDropDown *drop,GParamSpec *pspec,gpointer data)
{
    (void)pspec; RelationRow *row=data;
    PersonFactualRelationEditor *editor=g_object_get_data(G_OBJECT(drop),"editor");
    guint selected=gtk_drop_down_get_selected(drop);
    g_free(row->evidence_id);row->evidence_id=NULL;
    if(selected>0&&editor->evidence_ids&&selected<=editor->evidence_ids->len)
        row->evidence_id=g_strdup(g_ptr_array_index(editor->evidence_ids,selected-1));
    g_clear_pointer(&row->run_id,g_free);
    GtkStringList *model=run_model(editor,row->evidence_id);
    gtk_drop_down_set_model(row->run,G_LIST_MODEL(model));
    gtk_drop_down_set_selected(row->run,0);
}
static void type_changed(GtkDropDown *drop,GParamSpec *pspec,gpointer data)
{
    (void)pspec;RelationRow *row=data;guint selected=gtk_drop_down_get_selected(drop);
    g_free(row->type);row->type=selected?g_strdup(
        person_vocabulary_adapter_relation_code(selected-1)):NULL;
}
static void run_changed(GtkDropDown *drop,GParamSpec *pspec,gpointer data)
{
    (void)pspec;RelationRow *row=data;
    PersonFactualRelationEditor *editor=g_object_get_data(G_OBJECT(drop),"editor");
    g_free(row->run_id);row->run_id=g_strdup(filtered_run_id(editor,
        row->evidence_id,gtk_drop_down_get_selected(drop)));
}
static void note_changed(GtkEditable *editable,gpointer data)
{
    RelationRow *row=data;g_free(row->note);
    row->note=g_strdup(gtk_editable_get_text(editable));
}
static void remove_clicked(GtkButton *button,gpointer data)
{
    PersonFactualRelationEditor *editor=data;
    GtkWidget *row=gtk_widget_get_ancestor(GTK_WIDGET(button),GTK_TYPE_LIST_BOX_ROW);
    gint index=row?gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row)):-1;
    if(index<0||(guint)index>=editor->rows->len)return;
    gtk_list_box_remove(editor->list,row);
    g_ptr_array_remove_index(editor->rows,(guint)index);update_empty(editor);
}
static void add_clicked(GtkButton *button,gpointer data)
{
    (void)button;PersonFactualRelationEditor *editor=data;
    RelationRow *state=g_new0(RelationRow,1);
    GtkWidget *row=gtk_list_box_row_new(),*box=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);
    GtkWidget *header=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
    GtkStringList *types=person_vocabulary_adapter_create_relation_labels();
    const char *placeholder[]={"Sélectionner un type…",NULL};
    gtk_string_list_splice(types,0,0,placeholder);
    GtkWidget *type=gtk_drop_down_new(G_LIST_MODEL(types),NULL);
    GtkWidget *remove=gtk_button_new_from_icon_name("list-remove-symbolic");
    gtk_widget_set_name(type,"factual-relation-type");
    gtk_widget_set_name(remove,"factual-relation-remove");
    gtk_widget_set_tooltip_text(remove,"Retirer cette relation préparée");
    gtk_widget_set_hexpand(type,TRUE);gtk_box_append(GTK_BOX(header),type);
    gtk_box_append(GTK_BOX(header),remove);gtk_box_append(GTK_BOX(box),header);
    GtkStringList *evidences=evidence_model(editor);
    state->evidence=GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(evidences),NULL));
    gtk_widget_set_name(GTK_WIDGET(state->evidence),"factual-relation-evidence");
    gtk_box_append(GTK_BOX(box),GTK_WIDGET(state->evidence));
    GtkStringList *runs=run_model(editor,NULL);
    state->run=GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(runs),NULL));
    gtk_widget_set_name(GTK_WIDGET(state->run),"factual-relation-ocr-run");
    gtk_box_append(GTK_BOX(box),GTK_WIDGET(state->run));
    GtkWidget *note=gtk_entry_new();gtk_entry_set_placeholder_text(GTK_ENTRY(note),
        "Note factuelle (facultative)");gtk_box_append(GTK_BOX(box),note);
    gtk_widget_set_name(note,"factual-relation-note");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row),box);
    gtk_list_box_append(editor->list,row);g_ptr_array_add(editor->rows,state);
    g_object_set_data(G_OBJECT(state->evidence),"editor",editor);
    g_object_set_data(G_OBJECT(state->run),"editor",editor);
    g_signal_connect(type,"notify::selected",G_CALLBACK(type_changed),state);
    g_signal_connect(state->evidence,"notify::selected",G_CALLBACK(evidence_changed),state);
    g_signal_connect(state->run,"notify::selected",G_CALLBACK(run_changed),state);
    g_signal_connect(note,"changed",G_CALLBACK(note_changed),state);
    g_signal_connect(remove,"clicked",G_CALLBACK(remove_clicked),editor);update_empty(editor);
}
PersonFactualRelationEditor *person_factual_relation_editor_new(void)
{
    PersonFactualRelationEditor *editor=g_new0(PersonFactualRelationEditor,1);
    editor->rows=g_ptr_array_new_with_free_func(row_free);
    editor->root=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
    gtk_widget_set_name(editor->root,"person-factual-relation-editor");
    GtkWidget *header=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    GtkWidget *title=gtk_label_new("Relations factuelles");
    gtk_widget_set_hexpand(title,TRUE);gtk_widget_set_halign(title,GTK_ALIGN_START);
    GtkWidget *add=gtk_button_new_with_label("Ajouter une relation");
    gtk_widget_set_name(add,"factual-relation-add");
    gtk_box_append(GTK_BOX(header),title);gtk_box_append(GTK_BOX(header),add);
    gtk_box_append(GTK_BOX(editor->root),header);
    editor->list=GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(editor->list,GTK_SELECTION_NONE);
    editor->empty=gtk_label_new("Aucune relation factuelle préparée.");
    gtk_box_append(GTK_BOX(editor->root),GTK_WIDGET(editor->list));
    gtk_box_append(GTK_BOX(editor->root),editor->empty);
    g_signal_connect(add,"clicked",G_CALLBACK(add_clicked),editor);update_empty(editor);
    return editor;
}
GtkWidget *person_factual_relation_editor_get_widget(PersonFactualRelationEditor *editor)
{return editor?editor->root:NULL;}
void person_factual_relation_editor_set_available_evidence(
 PersonFactualRelationEditor *editor,GtkStringList *labels,GPtrArray *ids)
{
    if(!editor)return;
    g_set_object(&editor->evidence_labels,labels);
    if(editor->evidence_ids)g_ptr_array_unref(editor->evidence_ids);
    editor->evidence_ids=ids?g_ptr_array_ref(ids):NULL;
    for(guint i=0;i<editor->rows->len;i++){
        RelationRow *row=g_ptr_array_index(editor->rows,i);
        GtkStringList *model=evidence_model(editor);
        gtk_drop_down_set_model(row->evidence,G_LIST_MODEL(model));
        gtk_drop_down_set_selected(row->evidence,0);
    }
}
void person_factual_relation_editor_set_available_ocr_runs(
 PersonFactualRelationEditor *editor,GPtrArray *runs)
{
    if(!editor)return;
    if(editor->runs)g_ptr_array_unref(editor->runs);
    editor->runs=runs?g_ptr_array_ref(runs):NULL;
    for(guint i=0;i<editor->rows->len;i++){
        RelationRow *row=g_ptr_array_index(editor->rows,i);
        GtkStringList *model=run_model(editor,row->evidence_id);
        gtk_drop_down_set_model(row->run,G_LIST_MODEL(model));
        g_clear_pointer(&row->run_id,g_free);gtk_drop_down_set_selected(row->run,0);
    }
}
gboolean person_factual_relation_editor_collect_relations(
 PersonFactualRelationEditor *editor,GPtrArray **relations,GError **error)
{
    if(!editor||!relations){g_set_error_literal(error,editor_error(),1,
        "Éditeur de relations factuelles invalide.");return FALSE;}
    GPtrArray *result=g_ptr_array_new_with_free_func(input_free);
    GHashTable *keys=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
    for(guint i=0;i<editor->rows->len;i++){
        RelationRow *row=g_ptr_array_index(editor->rows,i);char *note=g_strdup(row->note);
        if(note)g_strstrip(note);
        if(note&&!*note)g_clear_pointer(&note,g_free);
        if(!row->evidence_id||!row->type){g_free(note);g_set_error_literal(error,
            editor_error(),2,"Chaque relation doit avoir une preuve et un type explicites.");goto bad;}
        char *key=g_strdup_printf("%s\037%s\037%s\037%s",row->evidence_id,row->type,
            row->run_id?row->run_id:"",note?note:"");
        if(g_hash_table_contains(keys,key)){g_free(key);g_free(note);
            g_set_error_literal(error,editor_error(),3,
                "Cette relation factuelle est déjà préparée.");goto bad;}
        g_hash_table_add(keys,key);
        PersonCreationFactualRelationInput *input=g_new0(PersonCreationFactualRelationInput,1);
        input->evidence_selection_identifier=g_strdup(row->evidence_id);
        input->ocr_run_identifier=g_strdup(row->run_id);input->relation_type=g_strdup(row->type);
        input->factual_note=note;g_ptr_array_add(result,input);
    }
    g_hash_table_unref(keys);*relations=result;return TRUE;
bad:g_hash_table_unref(keys);g_ptr_array_unref(result);return FALSE;
}
guint person_factual_relation_editor_get_count(const PersonFactualRelationEditor *editor)
{return editor?editor->rows->len:0;}
gboolean person_factual_relation_editor_validate(
 PersonFactualRelationEditor *editor,GError **error)
{
    GPtrArray *relations=NULL;
    gboolean valid=person_factual_relation_editor_collect_relations(
        editor,&relations,error);
    if(relations)g_ptr_array_unref(relations);
    return valid;
}
void person_factual_relation_editor_append_summary(
 PersonFactualRelationEditor *editor,GString *summary)
{
    GPtrArray *relations=NULL;
    if(!summary||!person_factual_relation_editor_collect_relations(
        editor,&relations,NULL))return;
    g_string_append(summary,"\n\nRelations factuelles préparées");
    if(relations->len==0)g_string_append(summary,"\nAucune relation.");
    for(guint i=0;i<relations->len;i++){
        PersonCreationFactualRelationInput *relation=g_ptr_array_index(relations,i);
        g_string_append_printf(summary,"\n• %s — preuve %s%s%s",
            person_vocabulary_adapter_relation_label(relation->relation_type),
            relation->evidence_selection_identifier,
            relation->factual_note?" — ":"",relation->factual_note?relation->factual_note:"");
    }
    g_ptr_array_unref(relations);
}
void person_factual_relation_editor_free(PersonFactualRelationEditor *editor)
{
    if(!editor)return;
    g_ptr_array_unref(editor->rows);
    g_clear_object(&editor->evidence_labels);
    if(editor->evidence_ids)g_ptr_array_unref(editor->evidence_ids);
    if(editor->runs)g_ptr_array_unref(editor->runs);
    g_free(editor);
}
