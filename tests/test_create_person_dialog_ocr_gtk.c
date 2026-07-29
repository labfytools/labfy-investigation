#include "views/create_person_dialog.h"
#include "core/file_hash.h"
#include "core/person_creation_coordinator.h"
#include "dao/evidence_dao.h"
#include "dao/identity_ocr_dao.h"
#include "database/database.h"
#include "database/statement.h"
#include <cairo-pdf.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>

typedef struct {
    GtkApplication *application;
    GtkWindow *main_window;
    GtkWindow *dialog;
    TaskManager *task_manager;
    ToolRegistry *registry;
    char *root;
    char *database_path;
    guint phase;
    guint guard;
    guint completion_count;
    guint layout_index;
    gboolean layout_resize_pending;
    gboolean passed;
    gboolean persisted;
} OcrGtkContext;
static void remove_tree(const char *root)
{
    GDir *directory=g_dir_open(root,0,NULL);const char *name;
    if(directory==NULL)return;
    while((name=g_dir_read_name(directory))!=NULL){
        char *path=g_build_filename(root,name,NULL);
        if(g_file_test(path,G_FILE_TEST_IS_DIR))remove_tree(path);
        else g_unlink(path);
        g_free(path);
    }
    g_dir_close(directory);g_rmdir(root);
}

static GtkWidget *find_button(GtkWidget *widget, const char *label)
{
    if (GTK_IS_BUTTON(widget) &&
        g_strcmp0(gtk_button_get_label(GTK_BUTTON(widget)), label) == 0)
        return widget;
    for (GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
         child=gtk_widget_get_next_sibling(child)) {
        GtkWidget *match=find_button(child,label);
        if (match!=NULL)return match;
    }
    return NULL;
}
static GtkWidget *find_named(GtkWidget *widget,const char *name)
{
    if(g_strcmp0(gtk_widget_get_name(widget),name)==0)return widget;
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
        child=gtk_widget_get_next_sibling(child)){
        GtkWidget *match=find_named(child,name);
        if(match!=NULL)return match;
    }
    return NULL;
}
static GtkWidget *find_ocr_field_row(GtkWidget *widget, const char *code,
    guint order)
{
    const char *widget_code = g_object_get_data(
        G_OBJECT(widget), "identity-field-code");
    guint widget_order = GPOINTER_TO_UINT(g_object_get_data(
        G_OBJECT(widget), "identity-field-order"));
    if (g_strcmp0(widget_code, code) == 0 && widget_order == order + 1)
        return widget;
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
        child=gtk_widget_get_next_sibling(child)){
        GtkWidget *match=find_ocr_field_row(child,code,order);
        if(match!=NULL)return match;
    }
    return NULL;
}
static guint count_ocr_field_rows(GtkWidget *widget)
{
    guint count=g_object_get_data(
        G_OBJECT(widget),"identity-field-code")!=NULL?1:0;
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
        child=gtk_widget_get_next_sibling(child))
        count+=count_ocr_field_rows(child);
    return count;
}
static GtkWidget *find_entry(GtkWidget *widget,const char *placeholder)
{
    if(GTK_IS_ENTRY(widget)&&g_strcmp0(
        gtk_entry_get_placeholder_text(GTK_ENTRY(widget)),placeholder)==0)
        return widget;
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
        child=gtk_widget_get_next_sibling(child)){
        GtkWidget *match=find_entry(child,placeholder);
        if(match!=NULL)return match;
    }
    return NULL;
}
static GtkWidget *find_label(GtkWidget *widget,const char *text);
static void diagnose_ocr_field(GtkWidget *root, guint phase,
    const char *code, guint order, GtkWidget *row)
{
    GtkWidget *entry=row!=NULL?g_object_get_data(
        G_OBJECT(row),"identity-value-entry"):NULL;
    GtkWidget *modify=row!=NULL?g_object_get_data(
        G_OBJECT(row),"identity-modify-button"):NULL;
    GtkWidget *reedit=row!=NULL?g_object_get_data(
        G_OBJECT(row),"identity-reedit-button"):NULL;
    const char *task_state=find_label(root,"OCR terminé")!=NULL
        ?"terminée":"en cours ou en échec";
    g_printerr("Diagnostic OCR: étape=%u propositions=%u "
        "code=%s ordre=%u ligne=%s entrée=%s modifier=%s "
        "rééditer=%s tâche=%s\n",phase,count_ocr_field_rows(root),code,order,
        row!=NULL?"oui":"non",entry!=NULL?"oui":"non",
        modify!=NULL?"oui":"non",reedit!=NULL?"oui":"non",task_state);
}
static GtkDropDown *find_evidence_dropdown(GtkWidget *widget)
{
    if(GTK_IS_DROP_DOWN(widget)){
        GListModel *model=gtk_drop_down_get_model(GTK_DROP_DOWN(widget));
        GtkStringObject *first=model!=NULL&&g_list_model_get_n_items(model)>0
            ?g_list_model_get_item(model,0):NULL;
        gboolean match=first!=NULL&&g_strcmp0(
            gtk_string_object_get_string(first),"Aucune preuve associée")==0;
        g_clear_object(&first);if(match)return GTK_DROP_DOWN(widget);
    }
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
        child=gtk_widget_get_next_sibling(child)){
        GtkDropDown *match=find_evidence_dropdown(child);
        if(match!=NULL)return match;
    }
    return NULL;
}
static GtkSpinButton *find_visible_spin(GtkWidget *widget)
{
    if(GTK_IS_SPIN_BUTTON(widget)&&gtk_widget_get_mapped(widget))
        return GTK_SPIN_BUTTON(widget);
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
        child=gtk_widget_get_next_sibling(child)){
        GtkSpinButton *match=find_visible_spin(child);
        if(match!=NULL)return match;
    }
    return NULL;
}
static GtkWidget *find_label(GtkWidget *widget,const char *text)
{
    if(GTK_IS_LABEL(widget)&&strstr(gtk_label_get_text(GTK_LABEL(widget)),
        text)!=NULL)return widget;
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child!=NULL;
        child=gtk_widget_get_next_sibling(child)){
        GtkWidget *match=find_label(child,text);
        if(match!=NULL)return match;
    }
    return NULL;
}
static char *text_view_contents(GtkTextView *view)
{
    GtkTextIter start,end;GtkTextBuffer *buffer=gtk_text_view_get_buffer(view);
    gtk_text_buffer_get_bounds(buffer,&start,&end);
    return gtk_text_buffer_get_text(buffer,&start,&end,FALSE);
}
static guint64 table_count(const char *database_path,const char *table)
{
    Database *database=database_open(database_path);
    char *sql=g_strdup_printf("SELECT COUNT(*) FROM %s;",table);
    DatabaseStatement *statement=database_statement_prepare(database,sql);
    int64_t value=0;g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement),==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_int64(statement,0,&value));
    database_statement_finalize(statement);database_close(database);
    g_free(sql);return (guint64)value;
}
static void verify_persisted_review(OcrGtkContext *context)
{
    GError *error=NULL;
    Database *database=database_open(context->database_path);
    IdentityOcrDao *dao=identity_ocr_dao_new(database);
    GPtrArray *runs=identity_ocr_dao_list_runs_by_evidence(dao,
        "10000000-0000-4000-8000-000000000002",&error);
    g_assert_no_error(error);g_assert_cmpuint(runs->len,==,1);
    IdentityOcrRunRecord *record=g_ptr_array_index(runs,0);
    IdentityOcrRun *run=identity_ocr_dao_load_run(dao,context->root,
        record->id,NULL,&error);
    g_assert_no_error(error);g_assert_nonnull(run);
    g_assert_cmpstr(identity_ocr_run_get_corrected_transcription(run),==,
        "TRANSCRIPTION PERSONNE SPECIMEN CORRIGÉE");
    g_assert_nonnull(strstr(identity_ocr_run_get_factual_notes(run),
        "note documentaire SPECIMEN"));
    const GPtrArray *fields=identity_ocr_run_get_fields(run);
    gboolean modified=FALSE,manual=FALSE;
    for(guint index=0;index<fields->len;index++){
        IdentityFieldObservation *field=g_ptr_array_index(
            (GPtrArray *)fields,index);
        modified|=g_strcmp0(
            identity_field_observation_get_corrected_value(field),
            "PAGE 2 VALEUR FINALE")==0&&
            g_strcmp0(identity_field_observation_get_origin(field),
                "manual_override")==0;
        manual|=g_strcmp0(
            identity_field_observation_get_corrected_value(field),
            "MANUEL SPECIMEN MODIFIÉ")==0&&
            g_strcmp0(identity_field_observation_get_origin(field),
                "manual_entry")==0;
    }
    g_assert_true(modified);g_assert_true(manual);
    identity_ocr_run_free(run);g_ptr_array_unref(runs);
    identity_ocr_dao_free(dao);database_close(database);
}
static void completed(CreatePersonDialogResult *result,gpointer data)
{
    OcrGtkContext *context=data;
    if(result!=NULL){
        GError *error=NULL;
        Database *database=database_open(context->database_path);
        PersonCreationCoordinatorResult *created=
            person_creation_coordinator_execute(database,context->root,
                create_person_dialog_result_get_input(result),
                create_person_dialog_result_get_evidence_selection(result),
                create_person_dialog_result_get_ocr_runs(result),
                NULL,&error);
        g_assert_no_error(error);g_assert_nonnull(created);
        person_creation_coordinator_result_free(created);
        database_close(database);
        create_person_dialog_result_free(result);
        context->persisted=TRUE;
    }
    context->completion_count++;
}
static gboolean drive(gpointer data)
{
    OcrGtkContext *context=data;
    GtkWidget *root=context->phase<4?GTK_WIDGET(context->dialog):NULL;
    GtkWidget *button;char *text;
    if(++context->guard>1000)g_error("Timeout du parcours OCR GTK");
    switch(context->phase){
    case 0:
        {
            static const int widths[]={1536,1366,1200,760};
            static const int heights[]={864,768,800,560};
            if(context->layout_index<G_N_ELEMENTS(widths)){
                if(!context->layout_resize_pending){
                    gtk_window_set_default_size(context->dialog,
                        widths[context->layout_index],
                        heights[context->layout_index]);
                    context->layout_resize_pending=TRUE;
                    return G_SOURCE_CONTINUE;
                }
                int width=0,height=0;
                gtk_window_get_default_size(context->dialog,&width,&height);
                g_assert_cmpint(width,==,widths[context->layout_index]);
                g_assert_cmpint(height,==,heights[context->layout_index]);
                GtkWidget *paned=find_named(root,
                    "create-person-ocr-paned");
                GtkWidget *scroll=find_named(root,
                    "create-person-ocr-scroll");
                GtkWidget *actions=find_named(root,
                    "create-person-actions");
                g_assert_true(GTK_IS_PANED(paned));
                g_assert_true(GTK_IS_SCROLLED_WINDOW(scroll));
                GtkPolicyType horizontal_policy=GTK_POLICY_ALWAYS;
                g_object_get(scroll,"hscrollbar-policy",
                    &horizontal_policy,NULL);
                g_assert_cmpint(horizontal_policy,==,GTK_POLICY_NEVER);
                g_assert_true(gtk_widget_get_mapped(actions));
                if(context->layout_index==0){
                    int position=gtk_paned_get_position(GTK_PANED(paned));
                    g_assert_cmpint(position,>=,600);
                    g_assert_cmpint(position,<=,680);
                }
                context->layout_index++;
                context->layout_resize_pending=FALSE;
                return G_SOURCE_CONTINUE;
            }
            GtkStack *transcription=GTK_STACK(find_named(root,
                "create-person-ocr-transcription-stack"));
            g_assert_true(GTK_IS_STACK(transcription));
            gtk_stack_set_visible_child_name(transcription,"corrected");
            g_assert_cmpstr(gtk_stack_get_visible_child_name(transcription),
                ==,"corrected");
            gtk_stack_set_visible_child_name(transcription,"raw");
            g_assert_cmpstr(gtk_stack_get_visible_child_name(transcription),
                ==,"raw");
        }
        if(find_button(root,"Analyser comme document d’identité")==NULL)
            return G_SOURCE_CONTINUE;
        if(!gtk_widget_get_sensitive(find_button(root,
            "Analyser comme document d’identité")))return G_SOURCE_CONTINUE;
        g_signal_emit_by_name(find_button(root,
            "Analyser comme document d’identité"),"clicked");
        context->phase++;break;
    case 1:
        if(find_label(root,"OCR terminé")==NULL)return G_SOURCE_CONTINUE;
        text=text_view_contents(GTK_TEXT_VIEW(find_named(
            root,"create-person-ocr-raw-text")));
        g_assert_nonnull(strstr(text,"NOM : SPECIMEN"));g_free(text);
        g_assert_nonnull(find_button(root,"Voir la zone"));
        g_assert_cmpuint(count_ocr_field_rows(root),>=,3);
        GtkWidget *raw_row=find_ocr_field_row(root,"nationality",3);
        diagnose_ocr_field(root,context->phase,"nationality",3,raw_row);
        g_assert_nonnull(raw_row);
        GtkWidget *raw_entry=g_object_get_data(
            G_OBJECT(raw_row),"identity-value-entry");
        GtkWidget *raw_modify=g_object_get_data(
            G_OBJECT(raw_row),"identity-modify-button");
        g_assert_nonnull(raw_entry);g_assert_nonnull(raw_modify);
        g_assert_true(gtk_editable_get_editable(GTK_EDITABLE(raw_entry)));
        gtk_editable_set_text(GTK_EDITABLE(raw_entry),"FRANÇAIS");
        g_signal_emit_by_name(raw_modify,"clicked");
        g_assert_nonnull(find_button(raw_row,"Modifiée"));
        g_assert_false(gtk_editable_get_editable(GTK_EDITABLE(raw_entry)));
        g_signal_emit_by_name(g_object_get_data(
            G_OBJECT(raw_row),"identity-reedit-button"),"clicked");
        g_assert_true(gtk_editable_get_editable(GTK_EDITABLE(raw_entry)));
        gtk_editable_set_text(GTK_EDITABLE(raw_entry),"FRANÇAIS CORRIGÉ");
        g_signal_emit_by_name(raw_modify,"clicked");
        g_assert_nonnull(find_label(raw_row,
            "Valeur OCR brute : FRANCAIS"));
        g_signal_emit_by_name(find_button(raw_row,
            "Revenir à la valeur OCR"),"clicked");
        g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(raw_entry)),==,
            "FRANCAIS");
        gtk_editable_set_text(GTK_EDITABLE(raw_entry),"FRANÇAIS FINAL");
        g_signal_emit_by_name(raw_modify,"clicked");
        g_signal_emit_by_name(find_button(raw_row,"Rejeter"),"clicked");
        g_signal_emit_by_name(find_button(raw_row,
            "Modifier à nouveau"),"clicked");
        g_signal_emit_by_name(raw_modify,"clicked");
        GtkWidget *manual_entry=find_entry(root,
            "Valeur réellement visible dans la preuve");
        g_assert_nonnull(manual_entry);
        gtk_editable_set_text(GTK_EDITABLE(manual_entry),
            "DOCUMENT VISIBLE SPECIMEN");
        g_signal_emit_by_name(find_button(root,
            "Ajouter un champ manquant"),"clicked");
        GtkWidget *manual_row=find_ocr_field_row(root,"document_type",4);
        diagnose_ocr_field(root,context->phase,"document_type",4,manual_row);
        g_assert_nonnull(manual_row);
        g_assert_nonnull(find_label(manual_row,"Origine : manual_entry"));
        g_signal_emit_by_name(g_object_get_data(
            G_OBJECT(manual_row),"identity-modify-button"),"clicked");
        GtkTextView *notes=GTK_TEXT_VIEW(find_named(
            root,"create-person-ocr-factual-notes"));
        g_assert_nonnull(notes);
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(notes),
            "document tronqué", -1);
        g_signal_emit_by_name(find_button(root,"Voir la zone"),"clicked");
        g_assert_true(create_person_dialog_test_overlay_has_region(
            context->dialog));
        button=find_button(root,"Accepter");
        g_signal_emit_by_name(button,"clicked");
        GtkWidget *second_row=find_ocr_field_row(root,"surname",1);
        diagnose_ocr_field(root,context->phase,"surname",1,second_row);
        g_assert_nonnull(second_row);
        button=g_object_get_data(
            G_OBJECT(second_row),"identity-modify-button");
        g_assert_nonnull(button);
        GtkWidget *entry=g_object_get_data(
            G_OBJECT(second_row),"identity-value-entry");
        g_assert_nonnull(entry);
        gtk_editable_set_text(GTK_EDITABLE(entry),"ALICE MODIFIÉE");
        g_signal_emit_by_name(button,"clicked");
        GtkWidget *third_row=find_ocr_field_row(root,"surname",2);
        diagnose_ocr_field(root,context->phase,"surname",2,third_row);
        g_assert_nonnull(third_row);
        button=g_object_get_data(
            G_OBJECT(third_row),"identity-reject-button");
        g_assert_nonnull(button);g_signal_emit_by_name(button,"clicked");
        g_assert_null(find_button(root,"Tout accepter"));
        g_assert_nonnull(find_button(root,"Acceptée"));
        g_assert_nonnull(find_button(root,"Modifiée"));
        g_assert_nonnull(find_button(root,"Rejetée"));
        g_signal_emit_by_name(find_button(root,"Précédent"),"clicked");
        g_signal_emit_by_name(find_button(root,"Suivant"),"clicked");
        g_assert_nonnull(find_button(root,"Acceptée"));
        guint64 generation=create_person_dialog_test_ocr_generation(
            context->dialog);
        button=find_button(root,"Analyser comme document d’identité");
        g_signal_emit_by_name(button,"clicked");
        g_signal_emit_by_name(button,"clicked");
        g_assert_cmpuint(create_person_dialog_test_ocr_generation(
            context->dialog),==,generation+2);
        context->phase++;break;
    case 2:
        if(find_label(root,"OCR terminé")==NULL)return G_SOURCE_CONTINUE;
        g_signal_emit_by_name(find_button(root,"Précédent"),"clicked");
        g_signal_emit_by_name(find_button(root,
            "Retirer"),"clicked");
        g_assert_false(create_person_dialog_test_overlay_has_region(
            context->dialog));
        g_signal_emit_by_name(find_button(root,"Suivant"),"clicked");
        text=text_view_contents(GTK_TEXT_VIEW(find_named(
            root,"create-person-ocr-raw-text")));
        g_assert_cmpstr(text,==,"");g_free(text);
        g_signal_emit_by_name(find_button(root,"Précédent"),"clicked");
        GtkDropDown *evidence=find_evidence_dropdown(root);
        g_assert_nonnull(evidence);gtk_drop_down_set_selected(evidence,2);
        g_signal_emit_by_name(find_button(root,
            "Ajouter à la sélection"),"clicked");
        g_signal_emit_by_name(find_button(root,"Suivant"),"clicked");
        GtkSpinButton *page=find_visible_spin(root);g_assert_nonnull(page);
        gtk_spin_button_set_value(page,2);
        g_signal_emit_by_name(find_button(root,
            "Analyser comme document d’identité"),"clicked");
        context->phase++;break;
    case 3:
        if(find_label(root,"OCR terminé")==NULL)return G_SOURCE_CONTINUE;
        {
            GtkStack *transcription=GTK_STACK(find_named(root,
                "create-person-ocr-transcription-stack"));
            GtkTextView *corrected=GTK_TEXT_VIEW(find_named(root,
                "identity-corrected-transcription"));
            GtkTextView *raw=GTK_TEXT_VIEW(find_named(root,
                "create-person-ocr-raw-text"));
            g_assert_cmpstr(gtk_stack_get_visible_child_name(transcription),
                ==,"corrected");
            g_assert_true(gtk_widget_get_mapped(GTK_WIDGET(corrected)));
            g_assert_true(gtk_widget_get_sensitive(GTK_WIDGET(corrected)));
            g_assert_true(gtk_text_view_get_editable(corrected));
            g_assert_false(gtk_text_view_get_editable(raw));
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(corrected),
                "TRANSCRIPTION PERSONNE SPECIMEN CORRIGÉE",-1);
            gtk_stack_set_visible_child_name(transcription,"raw");
            char *raw_text=text_view_contents(raw);
            g_assert_nonnull(strstr(raw_text,"NOM : SPECIMEN"));
            g_free(raw_text);
            gtk_stack_set_visible_child_name(transcription,"corrected");
            char *corrected_text=text_view_contents(corrected);
            g_assert_cmpstr(corrected_text,==,
                "TRANSCRIPTION PERSONNE SPECIMEN CORRIGÉE");
            g_free(corrected_text);
        }
        button=find_button(root,"Accepter");
        g_assert_nonnull(button);g_signal_emit_by_name(button,"clicked");
        GtkWidget *pdf_second_row=find_ocr_field_row(root,"surname",1);
        diagnose_ocr_field(root,context->phase,"surname",1,pdf_second_row);
        g_assert_nonnull(pdf_second_row);
        button=g_object_get_data(
            G_OBJECT(pdf_second_row),"identity-modify-button");
        g_assert_nonnull(button);
        GtkWidget *pdf_entry=g_object_get_data(
            G_OBJECT(pdf_second_row),"identity-value-entry");
        g_assert_nonnull(pdf_entry);
        g_assert_true(gtk_widget_get_sensitive(pdf_entry));
        g_assert_true(gtk_editable_get_editable(GTK_EDITABLE(pdf_entry)));
        char *pdf_raw_value=g_strdup(
            gtk_editable_get_text(GTK_EDITABLE(pdf_entry)));
        gtk_editable_set_text(GTK_EDITABLE(pdf_entry),"PAGE 2 PREMIÈRE");
        g_signal_emit_by_name(button,"clicked");
        g_signal_emit_by_name(g_object_get_data(
            G_OBJECT(pdf_second_row),"identity-reedit-button"),"clicked");
        g_assert_true(gtk_editable_get_editable(GTK_EDITABLE(pdf_entry)));
        gtk_editable_set_text(GTK_EDITABLE(pdf_entry),"PAGE 2 DEUXIÈME");
        g_signal_emit_by_name(button,"clicked");
        g_signal_emit_by_name(g_object_get_data(
            G_OBJECT(pdf_second_row),"identity-restore-button"),"clicked");
        g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(pdf_entry)),
            ==,pdf_raw_value);
        g_free(pdf_raw_value);
        gtk_editable_set_text(GTK_EDITABLE(pdf_entry),
            "PAGE 2 VALEUR FINALE");
        g_signal_emit_by_name(button,"clicked");
        GtkWidget *manual_input=find_entry(root,
            "Valeur réellement visible dans la preuve");
        gtk_editable_set_text(GTK_EDITABLE(manual_input),
            "MANUEL SPECIMEN INITIAL");
        g_signal_emit_by_name(find_button(root,
            "Ajouter un champ manquant"),"clicked");
        GtkWidget *pdf_manual_row=find_ocr_field_row(
            root,"document_type",4);
        g_assert_nonnull(pdf_manual_row);
        GtkWidget *manual_value=g_object_get_data(
            G_OBJECT(pdf_manual_row),"identity-value-entry");
        g_assert_true(gtk_widget_get_sensitive(manual_value));
        g_assert_true(gtk_editable_get_editable(GTK_EDITABLE(manual_value)));
        gtk_editable_set_text(GTK_EDITABLE(manual_value),
            "MANUEL SPECIMEN MODIFIÉ");
        g_signal_emit_by_name(g_object_get_data(
            G_OBJECT(pdf_manual_row),"identity-modify-button"),"clicked");
        GtkTextView *factual_notes=GTK_TEXT_VIEW(find_named(root,
            "create-person-ocr-factual-notes"));
        g_assert_true(gtk_text_view_get_editable(factual_notes));
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(factual_notes),
            "note documentaire SPECIMEN — document incomplet",-1);
        gtk_window_set_default_size(context->dialog,760,560);
        g_assert_true(gtk_widget_get_mapped(GTK_WIDGET(factual_notes)));
        GtkWidget *pdf_third_row=find_ocr_field_row(root,"surname",2);
        diagnose_ocr_field(root,context->phase,"surname",2,pdf_third_row);
        g_assert_nonnull(pdf_third_row);
        button=g_object_get_data(
            G_OBJECT(pdf_third_row),"identity-reject-button");
        g_assert_nonnull(button);g_signal_emit_by_name(button,"clicked");
        g_signal_emit_by_name(find_button(root,"Suivant"),"clicked");
        g_assert_nonnull(find_label(root,"page 2"));
        g_assert_nonnull(find_label(root,"PAGE 2 VALEUR FINALE"));
        g_assert_nonnull(find_label(root,"authenticité n’est pas établie"));
        g_assert_null(find_label(root,"brut : PAGE2"));
        g_signal_emit_by_name(find_button(root,
            "Créer la personne"),"clicked");
        context->phase++;break;
    case 4:
        if(context->completion_count!=1)return G_SOURCE_CONTINUE;
        g_assert_true(context->persisted);
        g_assert_cmpuint(table_count(context->database_path,"entites"),==,1);
        g_assert_cmpuint(table_count(context->database_path,
            "identity_ocr_runs"),==,1);
        verify_persisted_review(context);
        g_assert_true(gtk_widget_get_visible(GTK_WIDGET(context->main_window)));
        GPtrArray *empty=g_ptr_array_new();
        g_assert_true(create_person_dialog_present(context->main_window,empty,
            context->root,context->task_manager,
            tool_registry_find(context->registry,"tesseract"),NULL,completed,
            context,NULL));
        g_ptr_array_unref(empty);
        GList *windows=gtk_application_get_windows(context->application);
        context->dialog=windows->data==context->main_window
            ?GTK_WINDOW(windows->next->data):GTK_WINDOW(windows->data);
        g_signal_emit_by_name(find_button(GTK_WIDGET(context->dialog),
            "Annuler"),"clicked");
        context->phase++;break;
    case 5:
        if(context->completion_count!=2)return G_SOURCE_CONTINUE;
        g_assert_true(gtk_widget_get_visible(GTK_WIDGET(context->main_window)));
        context->passed=TRUE;
        g_application_quit(G_APPLICATION(context->application));
        return G_SOURCE_REMOVE;
    default:break;
    }
    return G_SOURCE_CONTINUE;
}
static void write_png(const char *path)
{
    cairo_surface_t *surface=cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32,320,160);
    cairo_t *cr=cairo_create(surface);cairo_set_source_rgb(cr,1,1,1);
    cairo_paint(cr);cairo_set_source_rgb(cr,0,0,0);
    cairo_move_to(cr,20,50);cairo_show_text(cr,"SPECIMEN OCR IDENTITE");
    g_assert_cmpint(cairo_surface_write_to_png(surface,path),==,
        CAIRO_STATUS_SUCCESS);cairo_destroy(cr);cairo_surface_destroy(surface);
}
static void write_pdf(const char *path)
{
    cairo_surface_t *surface=cairo_pdf_surface_create(path,320,160);
    cairo_t *cr=cairo_create(surface);
    cairo_move_to(cr,20,50);cairo_show_text(cr,"SPECIMEN PAGE UNE");
    cairo_show_page(cr);cairo_move_to(cr,20,50);
    cairo_show_text(cr,"SPECIMEN PAGE DEUX");cairo_show_page(cr);
    cairo_destroy(cr);cairo_surface_destroy(surface);
}
static void activate(GtkApplication *application,gpointer data)
{
    OcrGtkContext *context=data;GError *error=NULL;
    g_setenv("LABFY_FAKE_IDENTITY_OCR","1",TRUE);
    char *database_directory=g_build_filename(
        context->root,"00_BaseDeDonnees",NULL);
    context->database_path=g_build_filename(database_directory,
        "Enquete.sqlite",NULL);
    g_assert_cmpint(g_mkdir_with_parents(database_directory,0700),==,0);
    g_assert_true(database_initialize(context->database_path,
        "Enquête SPECIMEN GTK OCR",context->root));
    char *evidence_directory=g_build_filename(context->root,
        "01_Preuves_Originales","identity",NULL);
    g_assert_cmpint(g_mkdir_with_parents(evidence_directory,0700),==,0);
    char *png=g_build_filename(evidence_directory,"SPECIMEN.png",NULL);
    char *pdf=g_build_filename(evidence_directory,"SPECIMEN-page-2.pdf",NULL);
    write_png(png);write_pdf(pdf);
    char *png_sha=NULL,*pdf_sha=NULL;guint64 png_size=0,pdf_size=0;
    g_assert_true(file_hash_compute_sha256(png,NULL,&png_sha,&png_size,&error));
    g_assert_true(file_hash_compute_sha256(pdf,NULL,&pdf_sha,&pdf_size,&error));
    GPtrArray *records=g_ptr_array_new_with_free_func(
        (GDestroyNotify)evidence_record_free);
    EvidenceRecord *record=evidence_record_new(
        "10000000-0000-4000-8000-000000000001","SPECIMEN.png",
        "SPECIMEN.png","01_Preuves_Originales/identity/SPECIMEN.png",
        "document",png_size,png_sha,"2026-07-28T10:00:00Z",NULL,NULL,
        "SPECIMEN synthétique",EVIDENCE_INTEGRITY_STATUS_VALID,&error);
    g_assert_nonnull(record);evidence_record_set_display_metadata(
        record,"Document d’identité","image/png");g_ptr_array_add(records,record);
    record=evidence_record_new(
        "10000000-0000-4000-8000-000000000002","SPECIMEN-page-2.pdf",
        "SPECIMEN-page-2.pdf",
        "01_Preuves_Originales/identity/SPECIMEN-page-2.pdf",
        "document",pdf_size,pdf_sha,"2026-07-28T10:00:00Z",NULL,NULL,
        "PDF SPECIMEN multipage",EVIDENCE_INTEGRITY_STATUS_VALID,&error);
    g_assert_nonnull(record);evidence_record_set_display_metadata(
        record,"Document d’identité","application/pdf");
    g_ptr_array_add(records,record);g_assert_no_error(error);
    Database *database=database_open(context->database_path);
    EvidenceDao *evidence_dao=evidence_dao_new(database,&error);
    g_assert_no_error(error);g_assert_nonnull(evidence_dao);
    for(guint index=0;index<records->len;index++)
        g_assert_true(evidence_dao_insert(evidence_dao,
            g_ptr_array_index(records,index),&error));
    g_assert_no_error(error);
    evidence_dao_free(evidence_dao);database_close(database);
    context->registry=tool_registry_new();
    char *tool=g_canonicalize_filename("tests/fake_document_tool",NULL);
    g_assert_true(tool_registry_register(context->registry,"tesseract",
        "Tesseract synthétique",tool,TOOL_REQUIREMENT_OPTIONAL,&error));
    g_assert_true(tool_registry_refresh(context->registry,&error));
    g_assert_true(tool_registry_set_version(context->registry,"tesseract",
        "5.0.0 SPECIMEN",&error));g_assert_no_error(error);
    context->main_window=GTK_WINDOW(gtk_application_window_new(application));
    gtk_window_present(context->main_window);
    g_assert_true(create_person_dialog_present(context->main_window,records,
        context->root,context->task_manager,
        tool_registry_find(context->registry,"tesseract"),NULL,completed,
        context,NULL));
    GList *windows=gtk_application_get_windows(application);
    context->dialog=windows->data==context->main_window
        ?GTK_WINDOW(windows->next->data):GTK_WINDOW(windows->data);
    GtkWidget *designation=find_entry(GTK_WIDGET(context->dialog),
        "Personne présumée liée aux comptes");
    gtk_editable_set_text(GTK_EDITABLE(designation),"PERSONNE SPECIMEN");
    g_signal_emit_by_name(find_button(GTK_WIDGET(context->dialog),
        "Suivant"),"clicked");
    GtkWidget *role=find_button(GTK_WIDGET(context->dialog),"Victime");
    if(role!=NULL)g_signal_emit_by_name(role,"clicked");
    g_signal_emit_by_name(find_button(GTK_WIDGET(context->dialog),
        "Suivant"),"clicked");
    GtkDropDown *evidence=find_evidence_dropdown(GTK_WIDGET(context->dialog));
    gtk_drop_down_set_selected(evidence,1);
    g_signal_emit_by_name(find_button(GTK_WIDGET(context->dialog),
        "Ajouter à la sélection"),"clicked");
    g_signal_emit_by_name(find_button(GTK_WIDGET(context->dialog),
        "Suivant"),"clicked");
    g_timeout_add(10,drive,context);
    g_ptr_array_unref(records);g_free(database_directory);
    g_free(evidence_directory);g_free(png);g_free(pdf);
    g_free(png_sha);g_free(pdf_sha);g_free(tool);
}
int main(int argc,char **argv)
{
    if(!gtk_init_check()){g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;}
    OcrGtkContext context={0};
    context.root=g_dir_make_tmp("labfy-ocr-gtk-XXXXXX",NULL);
    context.application=gtk_application_new(
        "org.labfy.Investigation.OcrDialogTest",G_APPLICATION_NON_UNIQUE);
    context.task_manager=task_manager_new();
    g_signal_connect(context.application,"activate",G_CALLBACK(activate),
        &context);
    int status=g_application_run(G_APPLICATION(context.application),argc,argv);
    g_assert_true(context.passed);
    tool_registry_free(context.registry);task_manager_free(context.task_manager);
    g_object_unref(context.application);
    g_free(context.database_path);remove_tree(context.root);g_free(context.root);
    return status;
}
