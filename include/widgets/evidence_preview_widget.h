#ifndef LABFY_INVESTIGATION_EVIDENCE_PREVIEW_WIDGET_H
#define LABFY_INVESTIGATION_EVIDENCE_PREVIEW_WIDGET_H

#include "core/evidence_preview.h"
#include "core/task_manager.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct EvidencePreviewWidget EvidencePreviewWidget;
typedef gboolean (*EvidencePreviewWidgetSessionCheck)(gpointer user_data);

EvidencePreviewWidget *evidence_preview_widget_new(
    TaskManager *task_manager,
    EvidencePreviewWidgetSessionCheck session_check,
    gpointer session_data);
GtkWidget *evidence_preview_widget_get_widget(
    const EvidencePreviewWidget *widget);
void evidence_preview_widget_show(EvidencePreviewWidget *widget,
    const EvidencePreviewRequest *request, const char *metadata);
void evidence_preview_widget_clear(EvidencePreviewWidget *widget);
void evidence_preview_widget_cancel(EvidencePreviewWidget *widget);
void evidence_preview_widget_free(EvidencePreviewWidget *widget);
const char *evidence_preview_widget_get_state(
    const EvidencePreviewWidget *widget);
char *evidence_preview_widget_dup_text(
    const EvidencePreviewWidget *widget);

G_END_DECLS
#endif
