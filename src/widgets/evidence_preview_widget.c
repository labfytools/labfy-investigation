#include "widgets/evidence_preview_widget.h"
#include "core/evidence_preview_task.h"
#include "core/evidence_video_preview_controller.h"

struct EvidencePreviewWidget {
    GtkWidget *root;
    GtkLabel *metadata;
    GtkLabel *status;
    GtkWidget *toolbar;
    GtkStack *stack;
    GtkPicture *image;
    GtkPicture *pdf;
    GtkScrolledWindow *image_scroll;
    GtkScrolledWindow *pdf_scroll;
    GtkButton *zoom_out;
    GtkButton *fit;
    GtkButton *zoom_in;
    GtkLabel *zoom_label;
    GtkButton *previous_page;
    GtkButton *next_page;
    GtkLabel *page_label;
    GtkVideo *video;
    GtkTextView *text;
    GtkTextView *email;
    TaskManager *task_manager;
    BackgroundTask *task;
    EvidenceVideoPreviewController *video_controller;
    EvidencePreviewWidgetSessionCheck session_check;
    gpointer session_data;
    guint64 generation;
    char *identifier;
    EvidencePreviewRequest *request;
    char *metadata_text;
    gint raster_width;
    gint raster_height;
    guint zoom_index;
    gboolean fit_mode;
    gboolean raster_available;
    gboolean pdf_available;
    guint pdf_page;
    guint pdf_page_count;
};

typedef struct {
    GWeakRef root;
    char *identifier;
    guint64 generation;
} PreviewWidgetContext;

static void preview_widget_root_finalized(gpointer data, GObject *object)
{
    EvidencePreviewWidget *widget = data;
    (void) object;
    if (widget != NULL) widget->root = NULL;
}

static void preview_widget_media_pause(gpointer media)
{ gtk_media_stream_pause(GTK_MEDIA_STREAM(media)); }
static void preview_widget_media_seek(gpointer media)
{ gtk_media_stream_seek(GTK_MEDIA_STREAM(media), 0); }
static void preview_widget_media_detach(gpointer owner)
{ gtk_video_set_media_stream(GTK_VIDEO(owner), NULL); }
static void preview_widget_media_release(gpointer media)
{ g_object_unref(media); }

static void preview_widget_context_free(gpointer data)
{
    PreviewWidgetContext *context = data;
    if (context == NULL) return;
    g_weak_ref_clear(&context->root);
    g_free(context->identifier);
    g_free(context);
}

static void preview_widget_media_error(GtkMediaStream *media,
    GParamSpec *pspec, gpointer data)
{
    EvidencePreviewWidget *widget = data;
    const GError *error;
    (void) pspec;
    if (widget == NULL || widget->root == NULL) return;
    error = gtk_media_stream_get_error(media);
    if (error != NULL)
        gtk_label_set_text(widget->status,
            "Vidéo reconnue, mais codec de lecture indisponible.");
}

static void preview_widget_reset_content(EvidencePreviewWidget *widget)
{
    if (widget->video != NULL)
        evidence_video_preview_controller_stop(widget->video_controller);
    if (widget->image != NULL) gtk_picture_set_paintable(widget->image, NULL);
    if (widget->pdf != NULL) gtk_picture_set_paintable(widget->pdf, NULL);
    if (widget->text != NULL)
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(widget->text), "", -1);
    if (widget->email != NULL)
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(widget->email), "", -1);
}

static const double zoom_factors[] = {
    0.25, 0.50, 0.75, 1.00, 1.25, 1.50, 2.00, 3.00, 4.00
};

static GtkPicture *preview_widget_active_picture(
    EvidencePreviewWidget *widget)
{
    return widget->pdf_available ? widget->pdf : widget->image;
}

static GtkScrolledWindow *preview_widget_active_scroll(
    EvidencePreviewWidget *widget)
{
    return widget->pdf_available ? widget->pdf_scroll : widget->image_scroll;
}

static void preview_widget_update_page_controls(
    EvidencePreviewWidget *widget)
{
    char *text = g_strdup_printf("Page %u / %u",
        widget->pdf_page_count > 0 ? widget->pdf_page + 1U : 0U,
        widget->pdf_page_count);
    gtk_label_set_text(widget->page_label, text);
    g_free(text);
    gtk_widget_set_visible(GTK_WIDGET(widget->previous_page),
        widget->pdf_available);
    gtk_widget_set_visible(GTK_WIDGET(widget->next_page),
        widget->pdf_available);
    gtk_widget_set_visible(GTK_WIDGET(widget->page_label),
        widget->pdf_available);
    gtk_widget_set_sensitive(GTK_WIDGET(widget->previous_page),
        widget->pdf_available && widget->pdf_page > 0);
    gtk_widget_set_sensitive(GTK_WIDGET(widget->next_page),
        widget->pdf_available &&
        widget->pdf_page + 1U < widget->pdf_page_count);
}

static void preview_widget_apply_zoom(EvidencePreviewWidget *widget)
{
    GtkPicture *picture;
    GtkScrolledWindow *scroll;
    double factor;
    char *text;
    if (widget == NULL) return;
    picture = preview_widget_active_picture(widget);
    scroll = preview_widget_active_scroll(widget);
    gtk_widget_set_visible(GTK_WIDGET(widget->zoom_out),
        widget->raster_available);
    gtk_widget_set_visible(GTK_WIDGET(widget->fit),
        widget->raster_available);
    gtk_widget_set_visible(GTK_WIDGET(widget->zoom_in),
        widget->raster_available);
    gtk_widget_set_visible(GTK_WIDGET(widget->zoom_label),
        widget->raster_available);
    if (!widget->raster_available) return;
    factor = zoom_factors[widget->zoom_index];
    text = widget->fit_mode ? g_strdup("Ajusté") :
        g_strdup_printf("%.0f %%", factor * 100.0);
    gtk_label_set_text(widget->zoom_label, text);
    g_free(text);
    gtk_widget_set_sensitive(GTK_WIDGET(widget->zoom_out),
        widget->fit_mode || widget->zoom_index > 0);
    gtk_widget_set_sensitive(GTK_WIDGET(widget->zoom_in),
        widget->fit_mode ||
        widget->zoom_index + 1U < G_N_ELEMENTS(zoom_factors));
    gtk_widget_set_sensitive(GTK_WIDGET(widget->fit),
        !widget->fit_mode);
    if (widget->fit_mode) {
        gtk_picture_set_can_shrink(picture, TRUE);
        gtk_picture_set_content_fit(picture, GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_size_request(GTK_WIDGET(picture), -1, -1);
        gtk_widget_set_hexpand(GTK_WIDGET(picture), TRUE);
        gtk_widget_set_vexpand(GTK_WIDGET(picture), TRUE);
        gtk_scrolled_window_set_policy(scroll,
            GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    } else {
        gtk_picture_set_can_shrink(picture, FALSE);
        gtk_picture_set_content_fit(picture, GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_hexpand(GTK_WIDGET(picture), FALSE);
        gtk_widget_set_vexpand(GTK_WIDGET(picture), FALSE);
        gtk_widget_set_size_request(GTK_WIDGET(picture),
            MAX(1, (gint) (widget->raster_width * factor)),
            MAX(1, (gint) (widget->raster_height * factor)));
        gtk_scrolled_window_set_policy(scroll,
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    }
}

void evidence_preview_widget_zoom_in(EvidencePreviewWidget *widget)
{
    if (widget == NULL || !widget->raster_available) return;
    if (widget->fit_mode) {
        widget->fit_mode = FALSE;
        widget->zoom_index = 4;
    } else if (widget->zoom_index + 1U < G_N_ELEMENTS(zoom_factors))
        widget->zoom_index++;
    preview_widget_apply_zoom(widget);
}

void evidence_preview_widget_zoom_out(EvidencePreviewWidget *widget)
{
    if (widget == NULL || !widget->raster_available) return;
    if (widget->fit_mode) {
        widget->fit_mode = FALSE;
        widget->zoom_index = 2;
    } else if (widget->zoom_index > 0)
        widget->zoom_index--;
    preview_widget_apply_zoom(widget);
}

void evidence_preview_widget_fit(EvidencePreviewWidget *widget)
{
    if (widget == NULL || !widget->raster_available) return;
    widget->fit_mode = TRUE;
    preview_widget_apply_zoom(widget);
}

double evidence_preview_widget_get_zoom(
    const EvidencePreviewWidget *widget)
{
    return widget != NULL && !widget->fit_mode
        ? zoom_factors[widget->zoom_index] : 0.0;
}

gboolean evidence_preview_widget_is_fit(
    const EvidencePreviewWidget *widget)
{
    return widget != NULL && widget->fit_mode;
}

guint evidence_preview_widget_get_pdf_page(
    const EvidencePreviewWidget *widget)
{
    return widget != NULL ? widget->pdf_page : 0;
}

guint evidence_preview_widget_get_pdf_page_count(
    const EvidencePreviewWidget *widget)
{
    return widget != NULL ? widget->pdf_page_count : 0;
}

void evidence_preview_widget_cancel(EvidencePreviewWidget *widget)
{
    if (widget == NULL) return;
    widget->generation++;
    if (widget->task != NULL) {
        background_task_cancel(widget->task);
        background_task_unref(widget->task);
        widget->task = NULL;
    }
    if (widget->root != NULL)
        preview_widget_reset_content(widget);
    g_clear_pointer(&widget->identifier, g_free);
    g_clear_pointer(&widget->request, evidence_preview_request_free);
    g_clear_pointer(&widget->metadata_text, g_free);
}

void evidence_preview_widget_clear(EvidencePreviewWidget *widget)
{
    if (widget == NULL) return;
    evidence_preview_widget_cancel(widget);
    gtk_label_set_text(widget->metadata, "");
    gtk_label_set_text(widget->status, "Aucune preuve sélectionnée.");
    gtk_stack_set_visible_child_name(widget->stack, "empty");
    widget->raster_available = FALSE;
    widget->pdf_available = FALSE;
    widget->pdf_page = 0;
    widget->pdf_page_count = 0;
    preview_widget_apply_zoom(widget);
    preview_widget_update_page_controls(widget);
}

static void preview_widget_completed(BackgroundTask *task, gpointer data)
{
    PreviewWidgetContext *context = data;
    GtkWidget *root = GTK_WIDGET(g_weak_ref_get(&context->root));
    EvidencePreviewWidget *widget = root != NULL
        ? g_object_get_data(G_OBJECT(root), "evidence-preview-widget") : NULL;
    EvidencePreviewResult *result = background_task_get_result(task);
    gboolean current = widget != NULL &&
        widget->generation == context->generation &&
        g_strcmp0(widget->identifier, context->identifier) == 0 &&
        (widget->session_check == NULL ||
         widget->session_check(widget->session_data));
    if (current && evidence_preview_result_matches(result,
            context->identifier, context->generation)) {
        GError *error = NULL;
        if ((result->kind == EVIDENCE_PREVIEW_KIND_IMAGE ||
             result->kind == EVIDENCE_PREVIEW_KIND_PDF) &&
            result->png_bytes != NULL) {
            GdkTexture *texture = gdk_texture_new_from_bytes(
                result->png_bytes, &error);
            if (texture != NULL) {
                GtkPicture *picture = result->kind == EVIDENCE_PREVIEW_KIND_PDF
                    ? widget->pdf : widget->image;
                gtk_picture_set_paintable(picture, GDK_PAINTABLE(texture));
                gtk_stack_set_visible_child_name(widget->stack,
                    result->kind == EVIDENCE_PREVIEW_KIND_PDF
                    ? "pdf" : "image");
                gtk_label_set_text(widget->status, "Aperçu intègre.");
                widget->raster_available = TRUE;
                widget->pdf_available =
                    result->kind == EVIDENCE_PREVIEW_KIND_PDF;
                widget->raster_width = result->width;
                widget->raster_height = result->height;
                if (widget->pdf_available) {
                    widget->pdf_page_count = result->item_count;
                    widget->pdf_page = result->current_page - 1U;
                } else {
                    widget->pdf_page_count = 0;
                    widget->pdf_page = 0;
                }
                preview_widget_apply_zoom(widget);
                preview_widget_update_page_controls(widget);
                g_object_unref(texture);
            }
        } else if (result->kind == EVIDENCE_PREVIEW_KIND_VIDEO) {
            widget->raster_available = FALSE;
            widget->pdf_available = FALSE;
            GtkMediaStream *media = gtk_media_file_new_for_filename(
                result->controlled_path);
            gtk_media_stream_set_volume(media, 0.0);
            gtk_media_stream_pause(media);
            g_signal_connect(media, "notify::error",
                G_CALLBACK(preview_widget_media_error), widget);
            gtk_video_set_autoplay(widget->video, FALSE);
            gtk_video_set_media_stream(widget->video, media);
            evidence_video_preview_controller_replace(
                widget->video_controller, media);
            gtk_stack_set_visible_child_name(widget->stack, "video");
            gtk_label_set_text(widget->status, result->message);
        } else if (result->kind == EVIDENCE_PREVIEW_KIND_TEXT ||
                   result->kind == EVIDENCE_PREVIEW_KIND_EMAIL) {
            widget->raster_available = FALSE;
            widget->pdf_available = FALSE;
            GtkTextView *view = result->kind == EVIDENCE_PREVIEW_KIND_EMAIL
                ? widget->email : widget->text;
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(view),
                result->text != NULL ? result->text : "", -1);
            gtk_stack_set_visible_child_name(widget->stack,
                result->kind == EVIDENCE_PREVIEW_KIND_EMAIL
                ? "email" : "text");
            gtk_label_set_text(widget->status,
                result->truncated ? "Aperçu tronqué." : "Aperçu intègre.");
        } else {
            widget->raster_available = FALSE;
            widget->pdf_available = FALSE;
            gtk_stack_set_visible_child_name(widget->stack,
                result->kind == EVIDENCE_PREVIEW_KIND_UNSUPPORTED
                ? "unsupported" : "error");
            gtk_label_set_text(widget->status,
                result->message != NULL ? result->message :
                "Aperçu illisible.");
        }
        if (!widget->raster_available) {
            preview_widget_apply_zoom(widget);
            preview_widget_update_page_controls(widget);
        }
        if (error != NULL) {
            gtk_stack_set_visible_child_name(widget->stack, "error");
            gtk_label_set_text(widget->status, error->message);
        }
        g_clear_error(&error);
    } else if (widget != NULL && widget->generation == context->generation) {
        GError *error = background_task_dup_error(task);
        gtk_stack_set_visible_child_name(widget->stack, "error");
        gtk_label_set_text(widget->status,
            error != NULL ? error->message :
            "Aperçu annulé : l’enquête active a changé.");
        g_clear_error(&error);
    }
    if (widget != NULL && widget->task == task) {
        background_task_unref(widget->task);
        widget->task = NULL;
    }
    g_clear_object(&root);
}

static void preview_widget_start_request(EvidencePreviewWidget *widget,
    guint pdf_page)
{
    EvidencePreviewRequest *owned;
    PreviewWidgetContext *context;
    GError *error = NULL;
    if (widget == NULL || widget->request == NULL) return;
    widget->generation++;
    if (widget->task != NULL) {
        background_task_cancel(widget->task);
        background_task_unref(widget->task);
        widget->task = NULL;
    }
    owned = evidence_preview_request_new(
        widget->request->investigation_root_path,
        widget->request->evidence_identifier,
        widget->request->relative_path,
        widget->request->expected_sha256,
        widget->request->mime_type, widget->generation);
    evidence_preview_request_set_pdf_page(owned, pdf_page);
    gtk_label_set_text(widget->metadata,
        widget->metadata_text != NULL ? widget->metadata_text : "");
    gtk_label_set_text(widget->status,
        "Détection, vérification et chargement en cours…");
    gtk_stack_set_visible_child_name(widget->stack, "loading");
    context = g_new0(PreviewWidgetContext, 1);
    g_weak_ref_init(&context->root, G_OBJECT(widget->root));
    context->identifier = g_strdup(widget->identifier);
    context->generation = widget->generation;
    if (owned != NULL)
        widget->task = evidence_preview_task_start(widget->task_manager,
            owned, preview_widget_completed, context,
            preview_widget_context_free, &error);
    evidence_preview_request_free(owned);
    if (widget->task == NULL) {
        preview_widget_context_free(context);
        gtk_stack_set_visible_child_name(widget->stack, "error");
        gtk_label_set_text(widget->status,
            error != NULL ? error->message : "Aperçu impossible.");
    }
    g_clear_error(&error);
}

gboolean evidence_preview_widget_set_pdf_page(
    EvidencePreviewWidget *widget, guint page)
{
    if (widget == NULL || !widget->pdf_available ||
        page >= widget->pdf_page_count || page == widget->pdf_page)
        return FALSE;
    widget->pdf_page = page;
    preview_widget_update_page_controls(widget);
    preview_widget_start_request(widget, page);
    return TRUE;
}

static void preview_widget_on_zoom_out(
    GtkButton *button, gpointer data)
{
    (void) button;
    evidence_preview_widget_zoom_out(data);
}

static void preview_widget_on_fit(GtkButton *button, gpointer data)
{
    (void) button;
    evidence_preview_widget_fit(data);
}

static void preview_widget_on_zoom_in(
    GtkButton *button, gpointer data)
{
    (void) button;
    evidence_preview_widget_zoom_in(data);
}

static void preview_widget_on_previous(
    GtkButton *button, gpointer data)
{
    EvidencePreviewWidget *widget = data;
    (void) button;
    if (widget->pdf_page > 0)
        evidence_preview_widget_set_pdf_page(
            widget, widget->pdf_page - 1U);
}

static void preview_widget_on_next(GtkButton *button, gpointer data)
{
    EvidencePreviewWidget *widget = data;
    (void) button;
    if (widget->pdf_page + 1U < widget->pdf_page_count)
        evidence_preview_widget_set_pdf_page(
            widget, widget->pdf_page + 1U);
}

EvidencePreviewWidget *evidence_preview_widget_new(
    TaskManager *task_manager,
    EvidencePreviewWidgetSessionCheck session_check,
    gpointer session_data)
{
    EvidencePreviewWidget *widget;
    GtkWidget *text_scroll;
    GtkWidget *email_scroll;
    const EvidenceVideoPreviewActions actions = {
        preview_widget_media_pause, preview_widget_media_seek,
        preview_widget_media_detach, preview_widget_media_release
    };
    if (task_manager == NULL) return NULL;
    widget = g_new0(EvidencePreviewWidget, 1);
    widget->task_manager = task_manager;
    widget->session_check = session_check;
    widget->session_data = session_data;
    widget->fit_mode = TRUE;
    widget->zoom_index = 3;
    widget->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_name(widget->root, "evidence-preview-widget");
    g_object_weak_ref(G_OBJECT(widget->root),
        preview_widget_root_finalized, widget);
    widget->metadata = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(widget->metadata, 0.0f);
    gtk_label_set_selectable(widget->metadata, TRUE);
    widget->status = GTK_LABEL(gtk_label_new("Aucune preuve sélectionnée."));
    gtk_label_set_xalign(widget->status, 0.0f);
    widget->toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(widget->toolbar, "evidence-preview-toolbar");
    widget->zoom_out = GTK_BUTTON(gtk_button_new_from_icon_name(
        "zoom-out-symbolic"));
    widget->fit = GTK_BUTTON(gtk_button_new_from_icon_name(
        "zoom-fit-best-symbolic"));
    widget->zoom_in = GTK_BUTTON(gtk_button_new_from_icon_name(
        "zoom-in-symbolic"));
    widget->previous_page = GTK_BUTTON(gtk_button_new_from_icon_name(
        "go-previous-symbolic"));
    widget->next_page = GTK_BUTTON(gtk_button_new_from_icon_name(
        "go-next-symbolic"));
    widget->zoom_label = GTK_LABEL(gtk_label_new("Ajusté"));
    widget->page_label = GTK_LABEL(gtk_label_new("Page 0 / 0"));
    gtk_widget_set_name(GTK_WIDGET(widget->zoom_out),
        "evidence-preview-zoom-out");
    gtk_widget_set_name(GTK_WIDGET(widget->fit),
        "evidence-preview-fit");
    gtk_widget_set_name(GTK_WIDGET(widget->zoom_in),
        "evidence-preview-zoom-in");
    gtk_widget_set_name(GTK_WIDGET(widget->previous_page),
        "evidence-preview-previous-page");
    gtk_widget_set_name(GTK_WIDGET(widget->next_page),
        "evidence-preview-next-page");
    gtk_widget_set_name(GTK_WIDGET(widget->zoom_label),
        "evidence-preview-zoom-label");
    gtk_widget_set_name(GTK_WIDGET(widget->page_label),
        "evidence-preview-page-label");
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget->zoom_out), "Zoom arrière");
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget->fit),
        "Ajuster à la fenêtre");
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget->zoom_in), "Zoom avant");
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget->previous_page),
        "Page précédente");
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget->next_page),
        "Page suivante");
    gtk_box_append(GTK_BOX(widget->toolbar),
        GTK_WIDGET(widget->zoom_out));
    gtk_box_append(GTK_BOX(widget->toolbar), GTK_WIDGET(widget->fit));
    gtk_box_append(GTK_BOX(widget->toolbar), GTK_WIDGET(widget->zoom_in));
    gtk_box_append(GTK_BOX(widget->toolbar), GTK_WIDGET(widget->zoom_label));
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(widget->toolbar), spacer);
    gtk_box_append(GTK_BOX(widget->toolbar),
        GTK_WIDGET(widget->previous_page));
    gtk_box_append(GTK_BOX(widget->toolbar), GTK_WIDGET(widget->page_label));
    gtk_box_append(GTK_BOX(widget->toolbar),
        GTK_WIDGET(widget->next_page));
    g_signal_connect(widget->zoom_out, "clicked",
        G_CALLBACK(preview_widget_on_zoom_out), widget);
    g_signal_connect(widget->fit, "clicked",
        G_CALLBACK(preview_widget_on_fit), widget);
    g_signal_connect(widget->zoom_in, "clicked",
        G_CALLBACK(preview_widget_on_zoom_in), widget);
    g_signal_connect(widget->previous_page, "clicked",
        G_CALLBACK(preview_widget_on_previous), widget);
    g_signal_connect(widget->next_page, "clicked",
        G_CALLBACK(preview_widget_on_next), widget);
    widget->image = GTK_PICTURE(gtk_picture_new());
    widget->pdf = GTK_PICTURE(gtk_picture_new());
    widget->video = GTK_VIDEO(gtk_video_new());
    g_object_add_weak_pointer(G_OBJECT(widget->image),
        (gpointer *) &widget->image);
    g_object_add_weak_pointer(G_OBJECT(widget->pdf),
        (gpointer *) &widget->pdf);
    g_object_add_weak_pointer(G_OBJECT(widget->video),
        (gpointer *) &widget->video);
    for (guint i = 0; i < 2; i++) {
        GtkPicture *picture = i == 0 ? widget->image : widget->pdf;
        gtk_picture_set_can_shrink(picture, TRUE);
        gtk_picture_set_content_fit(picture, GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_hexpand(GTK_WIDGET(picture), TRUE);
        gtk_widget_set_vexpand(GTK_WIDGET(picture), TRUE);
    }
    widget->image_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    widget->pdf_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_widget_set_name(GTK_WIDGET(widget->image_scroll),
        "evidence-preview-image-scroll");
    gtk_widget_set_name(GTK_WIDGET(widget->pdf_scroll),
        "evidence-preview-pdf-scroll");
    gtk_widget_set_size_request(GTK_WIDGET(widget->image_scroll), 480, 260);
    gtk_widget_set_size_request(GTK_WIDGET(widget->pdf_scroll), 480, 260);
    gtk_widget_set_hexpand(GTK_WIDGET(widget->image_scroll), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(widget->image_scroll), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(widget->pdf_scroll), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(widget->pdf_scroll), TRUE);
    gtk_scrolled_window_set_child(widget->image_scroll,
        GTK_WIDGET(widget->image));
    gtk_scrolled_window_set_child(widget->pdf_scroll,
        GTK_WIDGET(widget->pdf));
    gtk_widget_set_size_request(GTK_WIDGET(widget->video), 480, 260);
    gtk_video_set_autoplay(widget->video, FALSE);
    widget->video_controller = evidence_video_preview_controller_new(
        widget->video, &actions);
    widget->text = GTK_TEXT_VIEW(gtk_text_view_new());
    widget->email = GTK_TEXT_VIEW(gtk_text_view_new());
    g_object_add_weak_pointer(G_OBJECT(widget->text),
        (gpointer *) &widget->text);
    g_object_add_weak_pointer(G_OBJECT(widget->email),
        (gpointer *) &widget->email);
    gtk_text_view_set_monospace(widget->text, TRUE);
    for (guint i = 0; i < 2; i++) {
        GtkTextView *view = i == 0 ? widget->text : widget->email;
        gtk_text_view_set_editable(view, FALSE);
        gtk_text_view_set_cursor_visible(view, FALSE);
        gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD_CHAR);
    }
    text_scroll = gtk_scrolled_window_new();
    email_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(text_scroll, 480, 260);
    gtk_widget_set_size_request(email_scroll, 480, 260);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(text_scroll),
        GTK_WIDGET(widget->text));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(email_scroll),
        GTK_WIDGET(widget->email));
    widget->stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(widget->stack, gtk_label_new("Aucun aperçu."), "empty");
    gtk_stack_add_named(widget->stack,
        gtk_label_new("Chargement de l’aperçu…"), "loading");
    gtk_stack_add_named(widget->stack,
        GTK_WIDGET(widget->image_scroll), "image");
    gtk_stack_add_named(widget->stack, GTK_WIDGET(widget->video), "video");
    gtk_stack_add_named(widget->stack, text_scroll, "text");
    gtk_stack_add_named(widget->stack, email_scroll, "email");
    gtk_stack_add_named(widget->stack,
        GTK_WIDGET(widget->pdf_scroll), "pdf");
    gtk_stack_add_named(widget->stack,
        gtk_label_new("Format reconnu mais aperçu indisponible."),
        "unsupported");
    gtk_stack_add_named(widget->stack,
        gtk_label_new("Erreur pendant la préparation de l’aperçu."), "error");
    gtk_stack_set_visible_child_name(widget->stack, "empty");
    gtk_box_append(GTK_BOX(widget->root), GTK_WIDGET(widget->metadata));
    gtk_box_append(GTK_BOX(widget->root), GTK_WIDGET(widget->status));
    gtk_box_append(GTK_BOX(widget->root), widget->toolbar);
    gtk_box_append(GTK_BOX(widget->root), GTK_WIDGET(widget->stack));
    preview_widget_apply_zoom(widget);
    preview_widget_update_page_controls(widget);
    g_object_set_data(G_OBJECT(widget->root),
        "evidence-preview-widget", widget);
    return widget;
}

GtkWidget *evidence_preview_widget_get_widget(
    const EvidencePreviewWidget *widget)
{ return widget != NULL ? widget->root : NULL; }

void evidence_preview_widget_show(EvidencePreviewWidget *widget,
    const EvidencePreviewRequest *request, const char *metadata)
{
    if (widget == NULL || request == NULL) return;
    evidence_preview_widget_cancel(widget);
    widget->identifier = g_strdup(request->evidence_identifier);
    widget->request = evidence_preview_request_new(
        request->investigation_root_path,
        request->evidence_identifier, request->relative_path,
        request->expected_sha256, request->mime_type, widget->generation);
    widget->metadata_text = g_strdup(metadata);
    widget->fit_mode = TRUE;
    widget->zoom_index = 3;
    widget->raster_available = FALSE;
    widget->pdf_available = FALSE;
    widget->pdf_page = request->pdf_page;
    widget->pdf_page_count = 0;
    preview_widget_apply_zoom(widget);
    preview_widget_update_page_controls(widget);
    preview_widget_start_request(widget, request->pdf_page);
}

void evidence_preview_widget_free(EvidencePreviewWidget *widget)
{
    if (widget == NULL) return;
    evidence_preview_widget_cancel(widget);
    if (widget->root != NULL) {
        g_object_set_data(G_OBJECT(widget->root),
            "evidence-preview-widget", NULL);
        g_object_weak_unref(G_OBJECT(widget->root),
            preview_widget_root_finalized, widget);
    }
    if (widget->video == NULL)
        evidence_video_preview_controller_abandon_owner(
            widget->video_controller);
    g_clear_pointer(&widget->video_controller,
        evidence_video_preview_controller_free);
    g_free(widget);
}

const char *evidence_preview_widget_get_state(
    const EvidencePreviewWidget *widget)
{
    return widget != NULL
        ? gtk_stack_get_visible_child_name(widget->stack) : NULL;
}

char *evidence_preview_widget_dup_text(
    const EvidencePreviewWidget *widget)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    if (widget == NULL) return NULL;
    buffer = gtk_text_view_get_buffer(widget->email);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}
