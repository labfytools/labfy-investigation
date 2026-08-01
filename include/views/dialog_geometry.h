#ifndef LABFY_DIALOG_GEOMETRY_H
#define LABFY_DIALOG_GEOMETRY_H

#include <gtk/gtk.h>

typedef struct {
    int initial_width;
    int initial_height;
    int minimum_width;
    int minimum_height;
} LabfyDialogGeometry;

LabfyDialogGeometry labfy_dialog_geometry_calculate(
    int workarea_width, int workarea_height);

void labfy_dialog_apply_standard_geometry(
    GtkWindow *window, GtkWindow *parent);

void labfy_dialog_prepare(
    GtkWindow *window,
    GtkWindow *parent,
    gboolean modal,
    gboolean destroy_with_parent);

void labfy_dialog_present(GtkWindow *window);

void labfy_paned_apply_initial_ratio(
    GtkPaned *paned, double ratio, int minimum_start, int minimum_end);
gboolean labfy_paned_initial_ratio_applied(GtkPaned *paned);

#endif
