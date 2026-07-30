/******************************************************************************
 * @file evidence_metadata_dialog.c
 * @brief Dialogue de modification des métadonnées éditables d'une preuve.
 ******************************************************************************/

#include "views/evidence_metadata_dialog.h"
#include "views/dialog_geometry.h"

#include "models/evidence_type.h"
#include "dao/identity_ocr_dao.h"
#include "views/evidence_identity_ocr_dialog.h"
#include "widgets/ocr_provenance_overlay.h"

struct EvidenceMetadataDialogResult
{
    char *type_identifier;
    char *source;
    char *description;
};

typedef struct
{
    GtkWindow *window;
    GtkDropDown *type_dropdown;
    GtkEditable *source_entry;
    GtkTextBuffer *description_buffer;
    GtkStringList *type_labels;
    GPtrArray *type_identifiers;
    EvidenceMetadataDialogCallback callback;
    EvidenceMetadataDialogAnalyzeCallback analyze_callback;
    char *evidence_identifier;
    char *investigation_root;
    GPtrArray *ocr_runs;
    GPtrArray *ocr_documents;
    GPtrArray *ocr_run_identifiers;
    GtkStringList *ocr_run_labels;
    GtkDropDown *ocr_run_dropdown;
    GtkLabel *ocr_summary;
    GtkTextBuffer *ocr_raw_buffer;
    GtkTextBuffer *ocr_corrected_buffer;
    GtkBox *ocr_details;
    GtkButton *ocr_revise_button;
    OcrProvenanceOverlay *ocr_overlay;
    gulong ocr_selection_handler;
    guint64 ocr_generation;
    gpointer user_data;
    gboolean completed;
} EvidenceMetadataDialogContext;

static void evidence_metadata_dialog_complete(
    EvidenceMetadataDialogContext *context,
    EvidenceMetadataDialogResult *result
);

/** @brief Ferme le dialogue avec Échap sans enregistrer. */
static gboolean evidence_metadata_dialog_on_key_pressed(
    GtkEventControllerKey *controller, guint keyval, guint keycode,
    GdkModifierType state, gpointer user_data
)
{
    (void) controller;
    (void) keycode;
    (void) state;
    if (keyval != GDK_KEY_Escape) return FALSE;
    evidence_metadata_dialog_complete(user_data, NULL);
    return TRUE;
}

/** @brief Copie un texte facultatif et transforme une chaîne vide en NULL. */
static char *evidence_metadata_dialog_copy_optional(const char *text)
{
    char *copy = text != NULL ? g_strdup(text) : NULL;
    if (copy != NULL) g_strstrip(copy);
    if (copy != NULL && copy[0] == '\0') g_clear_pointer(&copy, g_free);
    return copy;
}

/** @brief Libère le contexte détenu par la fenêtre. */
static void evidence_metadata_dialog_context_free(gpointer data)
{
    EvidenceMetadataDialogContext *context = data;
    if (context == NULL) return;
    g_clear_pointer(&context->type_identifiers, g_ptr_array_unref);
    g_clear_object(&context->type_labels);
    g_clear_pointer(&context->ocr_runs, g_ptr_array_unref);
    g_clear_pointer(&context->ocr_documents, g_ptr_array_unref);
    g_clear_pointer(&context->ocr_run_identifiers, g_ptr_array_unref);
    g_clear_object(&context->ocr_run_labels);
    g_free(context->investigation_root);
    g_free(context->evidence_identifier);
    g_free(context);
}

static void evidence_metadata_dialog_on_analyze(
    GtkButton *button, gpointer user_data)
{
    EvidenceMetadataDialogContext *context = user_data;
    (void) button;
    if (context != NULL && context->analyze_callback != NULL) {
        gboolean revise=GPOINTER_TO_INT(g_object_get_data(
            G_OBJECT(button),"revise-existing"));
        const char *run_identifier = NULL;
        if (revise && context->ocr_run_dropdown != NULL) {
            guint selected = gtk_drop_down_get_selected(
                context->ocr_run_dropdown);
            if (selected < context->ocr_run_identifiers->len)
                run_identifier = g_ptr_array_index(
                    context->ocr_run_identifiers, selected);
        }
        if (!revise || run_identifier != NULL)
            context->analyze_callback(context->window,
                context->evidence_identifier, revise, run_identifier,
                context->user_data);
        evidence_metadata_dialog_complete(context, NULL);
    }
}

/** @brief Détruit la fenêtre hors de la pile du signal GTK courant. */
static gboolean evidence_metadata_dialog_destroy_idle(gpointer data)
{
    GtkWindow *window = GTK_WINDOW(data);
    gtk_window_destroy(window);
    g_object_unref(window);
    return G_SOURCE_REMOVE;
}

/** @brief Termine le dialogue et transmet éventuellement un résultat. */
static void evidence_metadata_dialog_complete(
    EvidenceMetadataDialogContext *context,
    EvidenceMetadataDialogResult *result
)
{
    EvidenceMetadataDialogCallback callback = NULL;
    gpointer callback_data = NULL;
    if (context == NULL || context->completed) return;
    context->completed = TRUE;
    callback = context->callback;
    callback_data = context->user_data;
    gtk_widget_set_visible(GTK_WIDGET(context->window), FALSE);
    g_idle_add(
        evidence_metadata_dialog_destroy_idle,
        g_object_ref(context->window)
    );
    if (callback != NULL) callback(result, callback_data);
    else evidence_metadata_dialog_result_free(result);
}

/** @brief Annule la modification. */
static void evidence_metadata_dialog_on_cancel(
    GtkButton *button, gpointer user_data
)
{
    (void) button;
    evidence_metadata_dialog_complete(user_data, NULL);
}

/** @brief Traite la fermeture native comme une annulation explicite. */
static gboolean evidence_metadata_dialog_on_close_request(
    GtkWindow *window, gpointer user_data
)
{
    (void) window;
    evidence_metadata_dialog_complete(user_data, NULL);
    return TRUE;
}

/** @brief Construit et transmet les valeurs modifiées. */
static void evidence_metadata_dialog_on_save(
    GtkButton *button, gpointer user_data
)
{
    EvidenceMetadataDialogContext *context = user_data;
    EvidenceMetadataDialogResult *result = NULL;
    GtkTextIter start; GtkTextIter end; char *description = NULL;
    guint selected = 0U;
    (void) button;
    if (context == NULL) return;
    selected = gtk_drop_down_get_selected(context->type_dropdown);
    if (selected >= context->type_identifiers->len) return;
    gtk_text_buffer_get_bounds(context->description_buffer, &start, &end);
    description = gtk_text_buffer_get_text(
        context->description_buffer, &start, &end, FALSE);
    result = g_new0(EvidenceMetadataDialogResult, 1);
    result->type_identifier = g_strdup(
        g_ptr_array_index(context->type_identifiers, selected));
    result->source = evidence_metadata_dialog_copy_optional(
        gtk_editable_get_text(context->source_entry));
    result->description = evidence_metadata_dialog_copy_optional(description);
    g_free(description);
    evidence_metadata_dialog_complete(context, result);
}

static void evidence_metadata_dialog_clear_box(GtkBox *box)
{
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(box));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(box, child);
        child = next;
    }
}

static IdentityFieldObservation *evidence_metadata_dialog_field_model(
    const IdentityFieldObservationRecord *record)
{
    IdentitySourceBox box = {
        .page = (gint) record->page_number,
        .x = (gint) record->source_x,
        .y = (gint) record->source_y,
        .width = (gint) record->source_width,
        .height = (gint) record->source_height,
        .image_width = (gint) record->source_image_width,
        .image_height = (gint) record->source_image_height,
        .available = record->has_source_box
    };
    IdentityFieldObservation *field = record->raw_value != NULL
        ? identity_field_observation_new(record->field_code,
            record->raw_value,
            record->has_confidence ? record->confidence : -1.0,
            &box, (guint) record->display_order)
        : identity_field_observation_new_manual(record->field_code,
            record->corrected_value, (guint) record->display_order);
    if (field != NULL)
        identity_field_observation_set_origin(field, record->origin);
    return field;
}

static void evidence_metadata_dialog_show_region(
    GtkButton *button, gpointer user_data)
{
    EvidenceMetadataDialogContext *context = user_data;
    IdentityFieldObservationRecord *record = g_object_get_data(
        G_OBJECT(button), "ocr-field-record");
    IdentityFieldObservation *field =
        evidence_metadata_dialog_field_model(record);
    context->ocr_generation++;
    ocr_provenance_overlay_set_field(
        context->ocr_overlay, field, context->ocr_generation);
    identity_field_observation_free(field);
}

static void evidence_metadata_dialog_render_ocr(
    EvidenceMetadataDialogContext *context)
{
    guint selected = gtk_drop_down_get_selected(context->ocr_run_dropdown);
    evidence_metadata_dialog_clear_box(context->ocr_details);
    context->ocr_generation++;
    ocr_provenance_overlay_clear(
        context->ocr_overlay, context->ocr_generation);
    gtk_text_buffer_set_text(context->ocr_raw_buffer, "", -1);
    gtk_text_buffer_set_text(context->ocr_corrected_buffer, "", -1);
    gtk_label_set_text(context->ocr_summary,
        "Aucune analyse OCR sélectionnée.");
    if (context->ocr_revise_button != NULL)
        gtk_widget_set_sensitive(
            GTK_WIDGET(context->ocr_revise_button), FALSE);
    if (selected >= context->ocr_runs->len ||
        selected >= context->ocr_run_identifiers->len) return;

    IdentityOcrRunRecord *run = g_ptr_array_index(
        context->ocr_runs, selected);
    ocr_provenance_overlay_set_page(context->ocr_overlay,
        (guint) run->page_number, context->ocr_generation);
    const char *selected_identifier = g_ptr_array_index(
        context->ocr_run_identifiers, selected);
    if (g_strcmp0(run->id, selected_identifier) != 0) return;
    char *summary = g_strdup_printf(
        "Run actif : %s\nUTC : %s\nMoteur : %s %s\n"
        "Langues : %s\nDocument : %s — %s — page %" G_GINT64_FORMAT
        "\nStatut : %s\nSHA-256 preuve : %s\n"
        "Artefact texte : %s\nSHA-256 texte : %s\n"
        "Artefact TSV : %s\nSHA-256 TSV : %s",
        run->id, run->executed_at, run->engine, run->engine_version,
        run->requested_languages, run->document_type, run->document_side,
        run->page_number, run->status, run->expected_sha256,
        run->text_relative_path, run->text_sha256,
        run->tsv_relative_path, run->tsv_sha256);
    gtk_label_set_text(context->ocr_summary, summary);
    g_free(summary);
    char *raw_path = g_build_filename(context->investigation_root,
        run->text_relative_path, NULL);
    char *raw = NULL;
    if (!g_file_get_contents(raw_path, &raw, NULL, NULL))
        raw = g_strdup("Artefact OCR brut indisponible");
    gtk_text_buffer_set_text(context->ocr_raw_buffer, raw, -1);
    gtk_text_buffer_set_text(context->ocr_corrected_buffer,
        run->corrected_transcription != NULL
            ? run->corrected_transcription
            : "Aucune transcription corrigée", -1);
    g_free(raw_path);
    g_free(raw);

    IdentityDocumentObservationRecord *document = NULL;
    for (guint index = 0; index < context->ocr_documents->len; index++) {
        IdentityDocumentObservationRecord *candidate =
            g_ptr_array_index(context->ocr_documents, index);
        if (g_strcmp0(candidate->ocr_run_id, run->id) == 0) {
            document = candidate;
            break;
        }
    }
    if (document == NULL) {
        gtk_box_append(context->ocr_details,
            gtk_label_new("Observation OCR persistée indisponible."));
        return;
    }
    char *description = g_strdup_printf(
        "Observation %s\nPersonne liée : %s\nDocument : %s — %s — "
        "page %" G_GINT64_FORMAT "\nÉtat : %s\nNotes : %s",
        document->id,
        document->person_id != NULL ? document->person_id
            : "Personne indisponible",
        document->document_type, document->document_side,
        document->page_number, document->review_state,
        document->factual_notes != NULL ? document->factual_notes
            : "Aucune note");
    GtkWidget *document_label = gtk_label_new(description);
    gtk_widget_set_name(document_label, "evidence-ocr-observation");
    gtk_label_set_wrap(GTK_LABEL(document_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(document_label), 0.0f);
    gtk_box_append(context->ocr_details, document_label);
    g_free(description);

    IdentityOcrDao *dao = g_object_get_data(
        G_OBJECT(context->window), "identity-ocr-dao");
    GError *error = NULL;
    GPtrArray *fields = identity_ocr_dao_list_fields_by_document(
        dao, document->id, &error);
    for (guint index = 0; fields != NULL && index < fields->len; index++) {
        IdentityFieldObservationRecord *field =
            g_ptr_array_index(fields, index);
        char *field_text = g_strdup_printf(
            "%s — brut : %s — corrigé : %s — statut : %s — "
            "origine : %s — page %" G_GINT64_FORMAT,
            field->field_code,
            field->raw_value != NULL ? field->raw_value : "absent",
            field->corrected_value != NULL
                ? field->corrected_value : "absente",
            field->review_status, field->origin, field->page_number);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *label = gtk_label_new(field_text);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_box_append(GTK_BOX(row), label);
        GtkWidget *show = gtk_button_new_with_label("Voir la zone");
        gtk_widget_set_sensitive(show, field->has_source_box);
        g_object_set_data(G_OBJECT(show), "ocr-field-record", field);
        g_signal_connect(show, "clicked",
            G_CALLBACK(evidence_metadata_dialog_show_region), context);
        gtk_box_append(GTK_BOX(row), show);
        gtk_box_append(context->ocr_details, row);
        g_free(field_text);
    }
    g_object_set_data_full(G_OBJECT(context->ocr_details),
        "active-ocr-fields", fields, fields != NULL
            ? (GDestroyNotify) g_ptr_array_unref : NULL);
    g_clear_error(&error);
    if (context->ocr_revise_button != NULL)
        gtk_widget_set_sensitive(
            GTK_WIDGET(context->ocr_revise_button), TRUE);
}

static void evidence_metadata_dialog_ocr_selected(
    GObject *object, GParamSpec *spec, gpointer user_data)
{
    (void) object;
    (void) spec;
    evidence_metadata_dialog_render_ocr(user_data);
}

static GtkWidget *evidence_metadata_dialog_build_ocr(
    EvidenceMetadataDialogContext *context, Database *database,
    const char *root, const EvidenceRecord *record)
{
    if (database == NULL || root == NULL) return NULL;
    GError *error = NULL;
    IdentityOcrDao *dao = identity_ocr_dao_new(database);
    context->ocr_runs = identity_ocr_dao_list_runs_by_evidence(
        dao, evidence_record_get_identifier(record), &error);
    context->ocr_documents = identity_ocr_dao_list_documents_by_evidence(
        dao, evidence_record_get_identifier(record), &error);
    if (context->ocr_runs == NULL || context->ocr_runs->len == 0 ||
        context->ocr_documents == NULL) {
        g_clear_error(&error);
        identity_ocr_dao_free(dao);
        return NULL;
    }
    g_object_set_data_full(G_OBJECT(context->window), "identity-ocr-dao",
        dao, (GDestroyNotify) identity_ocr_dao_free);
    context->investigation_root = g_strdup(root);
    context->ocr_run_identifiers =
        g_ptr_array_new_with_free_func(g_free);
    context->ocr_run_labels = gtk_string_list_new(NULL);
    for (guint index = 0; index < context->ocr_runs->len; index++) {
        IdentityOcrRunRecord *run =
            g_ptr_array_index(context->ocr_runs, index);
        char *short_id = g_strndup(run->id, 8);
        char *label = g_strdup_printf(
            "%s — %s — %s — %s/%s — p.%" G_GINT64_FORMAT
            " — %s — %s",
            run->executed_at, run->engine, run->requested_languages,
            run->document_type, run->document_side, run->page_number,
            run->status, short_id);
        gtk_string_list_append(context->ocr_run_labels, label);
        g_ptr_array_add(context->ocr_run_identifiers, g_strdup(run->id));
        g_free(short_id);
        g_free(label);
    }
    GtkWidget *section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_name(section, "evidence-ocr-readonly");
    gtk_box_append(GTK_BOX(section), gtk_label_new(
        "Analyse OCR d’identité"));
    context->ocr_run_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(g_object_ref(context->ocr_run_labels)), NULL));
    gtk_widget_set_name(GTK_WIDGET(context->ocr_run_dropdown),
        "evidence-ocr-run-selector");
    gtk_box_append(GTK_BOX(section),
        GTK_WIDGET(context->ocr_run_dropdown));
    context->ocr_summary = GTK_LABEL(gtk_label_new(NULL));
    gtk_widget_set_name(GTK_WIDGET(context->ocr_summary),
        "evidence-ocr-active-run");
    gtk_label_set_wrap(context->ocr_summary, TRUE);
    gtk_label_set_xalign(context->ocr_summary, 0.0f);
    gtk_box_append(GTK_BOX(section), GTK_WIDGET(context->ocr_summary));
    GtkWidget *raw = gtk_text_view_new();
    gtk_widget_set_name(raw, "evidence-ocr-raw");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(raw), FALSE);
    context->ocr_raw_buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(raw));
    gtk_box_append(GTK_BOX(section), gtk_label_new("Texte OCR brut"));
    gtk_box_append(GTK_BOX(section), raw);
    GtkWidget *corrected = gtk_text_view_new();
    gtk_widget_set_name(corrected, "evidence-ocr-corrected");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(corrected), FALSE);
    context->ocr_corrected_buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(corrected));
    gtk_box_append(GTK_BOX(section),
        gtk_label_new("Transcription corrigée"));
    gtk_box_append(GTK_BOX(section), corrected);
    context->ocr_details = GTK_BOX(gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_name(GTK_WIDGET(context->ocr_details),
        "evidence-ocr-details");
    gtk_box_append(GTK_BOX(section), GTK_WIDGET(context->ocr_details));
    context->ocr_overlay = ocr_provenance_overlay_new();
    g_object_set_data_full(G_OBJECT(section), "ocr-readonly-overlay",
        context->ocr_overlay, (GDestroyNotify) ocr_provenance_overlay_free);
    gtk_box_append(GTK_BOX(section),
        ocr_provenance_overlay_get_widget(context->ocr_overlay));
    context->ocr_selection_handler = g_signal_connect(
        context->ocr_run_dropdown, "notify::selected",
        G_CALLBACK(evidence_metadata_dialog_ocr_selected), context);
    /*
     * Politique d'ouverture et de rafraîchissement : le dernier run inséré
     * devient actif. La révision, elle, lit toujours l'UUID de la sélection
     * courante et ne déduit jamais un run de cette politique.
     */
    gtk_drop_down_set_selected(context->ocr_run_dropdown,
        context->ocr_runs->len - 1);
    return section;
}

gboolean evidence_metadata_dialog_present_with_ocr(
    GtkWindow *parent, const EvidenceRecord *record,
    const GPtrArray *evidence_types, Database *database,
    const char *investigation_root,
    EvidenceMetadataDialogAnalyzeCallback analyze_callback,
    EvidenceMetadataDialogCallback callback, gpointer user_data
)
{
    EvidenceMetadataDialogContext *context = NULL;
    GtkStringList *labels = NULL; GtkWidget *main_box = NULL;
    GtkWidget *description_view = NULL; GtkWidget *description_scroll = NULL;
    GtkWidget *buttons = NULL; GtkWidget *cancel = NULL; GtkWidget *save = NULL;
    guint selected = 0U;
    if (record == NULL || evidence_types == NULL || evidence_types->len == 0U ||
        callback == NULL) return FALSE;
    context = g_new0(EvidenceMetadataDialogContext, 1);
    context->window = GTK_WINDOW(gtk_window_new());
    context->callback = callback; context->user_data = user_data;
    context->analyze_callback = analyze_callback;
    context->evidence_identifier =
        g_strdup(evidence_record_get_identifier(record));
    context->type_identifiers = g_ptr_array_new_with_free_func(g_free);
    labels = gtk_string_list_new(NULL);
    context->type_labels = labels;
    for (guint index = 0U; index < evidence_types->len; index++)
    {
        const EvidenceType *type = g_ptr_array_index((GPtrArray *) evidence_types, index);
        gtk_string_list_append(labels, evidence_type_get_label(type));
        g_ptr_array_add(context->type_identifiers,
            g_strdup(evidence_type_get_code(type)));
        if (g_strcmp0(evidence_type_get_code(type),
                evidence_record_get_type_identifier(record)) == 0) selected = index;
    }
    gtk_window_set_title(context->window, "Modifier les métadonnées");
    labfy_dialog_prepare(context->window, parent, TRUE, TRUE);
    if (parent != NULL) {
        gtk_window_set_application(context->window,
            gtk_window_get_application(parent));
    }
    g_object_set_data_full(G_OBJECT(context->window), "evidence-metadata-context",
        context, evidence_metadata_dialog_context_free);
    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 18); gtk_widget_set_margin_end(main_box, 18);
    gtk_widget_set_margin_top(main_box, 18); gtk_widget_set_margin_bottom(main_box, 18);
    gtk_box_append(GTK_BOX(main_box), gtk_label_new(
        "Le fichier, son UUID, sa taille, sa date d'import et son SHA-256 resteront inchangés."));
    context->type_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(g_object_ref(labels)), NULL));
    gtk_drop_down_set_selected(context->type_dropdown, selected);
    gtk_box_append(GTK_BOX(main_box), gtk_label_new("Type de preuve"));
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(context->type_dropdown));
    context->source_entry = GTK_EDITABLE(gtk_entry_new());
    gtk_editable_set_text(context->source_entry,
        evidence_record_get_source(record) != NULL
            ? evidence_record_get_source(record) : "");
    gtk_box_append(GTK_BOX(main_box), gtk_label_new("Source"));
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(context->source_entry));
    description_view = gtk_text_view_new();
    context->description_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(description_view));
    gtk_text_buffer_set_text(context->description_buffer,
        evidence_record_get_description(record) != NULL
            ? evidence_record_get_description(record) : "", -1);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(description_view), GTK_WRAP_WORD_CHAR);
    description_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(description_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(description_scroll), description_view);
    gtk_box_append(GTK_BOX(main_box), gtk_label_new("Description"));
    gtk_box_append(GTK_BOX(main_box), description_scroll);
    GtkWidget *ocr = evidence_metadata_dialog_build_ocr(
        context, database, investigation_root, record);
    if (ocr != NULL) {
        GtkWidget *ocr_scroll = gtk_scrolled_window_new();
        gtk_widget_set_size_request(ocr_scroll, -1, 260);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ocr_scroll), ocr);
        gtk_box_append(GTK_BOX(main_box), ocr_scroll);
    }
    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    cancel = gtk_button_new_with_label("Annuler");
    save = gtk_button_new_with_label("Enregistrer");
    if (analyze_callback != NULL && ocr != NULL) {
        GtkWidget *revise = gtk_button_new_with_label(
            "Réviser l’analyse OCR");
        context->ocr_revise_button = GTK_BUTTON(revise);
        gtk_widget_set_name(revise, "revise-existing-identity-ocr");
        g_object_set_data(G_OBJECT(revise),"revise-existing",
            GINT_TO_POINTER(TRUE));
        g_signal_connect(revise, "clicked",
            G_CALLBACK(evidence_metadata_dialog_on_analyze), context);
        gtk_box_append(GTK_BOX(buttons), revise);
        evidence_metadata_dialog_render_ocr(context);
    }
    if (analyze_callback != NULL &&
        evidence_identity_ocr_dialog_file_is_compatible(
            evidence_record_get_original_name(record))) {
        GtkWidget *analyze = gtk_button_new_with_label(
            "Relancer une nouvelle analyse OCR");
        gtk_widget_set_name(analyze, "analyze-existing-identity");
        g_signal_connect(analyze, "clicked",
            G_CALLBACK(evidence_metadata_dialog_on_analyze), context);
        gtk_box_append(GTK_BOX(buttons), analyze);
    }
    gtk_widget_add_css_class(save, "suggested-action");
    g_signal_connect(cancel, "clicked", G_CALLBACK(evidence_metadata_dialog_on_cancel), context);
    g_signal_connect(save, "clicked", G_CALLBACK(evidence_metadata_dialog_on_save), context);
    g_signal_connect(context->window, "close-request",
        G_CALLBACK(evidence_metadata_dialog_on_close_request), context);
    {
        GtkEventController *keys = gtk_event_controller_key_new();
        g_signal_connect(keys, "key-pressed",
            G_CALLBACK(evidence_metadata_dialog_on_key_pressed), context);
        gtk_widget_add_controller(GTK_WIDGET(context->window), keys);
    }
    gtk_box_append(GTK_BOX(buttons), cancel); gtk_box_append(GTK_BOX(buttons), save);
    gtk_box_append(GTK_BOX(main_box), buttons);
    gtk_window_set_child(context->window, main_box);
    labfy_dialog_present(context->window);
    return TRUE;
}

gboolean evidence_metadata_dialog_present(
    GtkWindow *parent, const EvidenceRecord *record,
    const GPtrArray *evidence_types, EvidenceMetadataDialogCallback callback,
    gpointer user_data)
{
    return evidence_metadata_dialog_present_with_ocr(
        parent, record, evidence_types, NULL, NULL, NULL,
        callback, user_data);
}

void evidence_metadata_dialog_result_free(EvidenceMetadataDialogResult *result)
{
    if (result == NULL) return;
    g_free(result->type_identifier); g_free(result->source);
    g_free(result->description); g_free(result);
}
const char *evidence_metadata_dialog_result_get_type_identifier(
    const EvidenceMetadataDialogResult *result)
{ return result != NULL ? result->type_identifier : NULL; }
const char *evidence_metadata_dialog_result_get_source(
    const EvidenceMetadataDialogResult *result)
{ return result != NULL ? result->source : NULL; }
const char *evidence_metadata_dialog_result_get_description(
    const EvidenceMetadataDialogResult *result)
{ return result != NULL ? result->description : NULL; }
