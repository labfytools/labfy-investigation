#include "views/dialog_geometry.h"

#define LABFY_DIALOG_INITIAL_WIDTH 1200
#define LABFY_DIALOG_INITIAL_HEIGHT 800
#define LABFY_DIALOG_MINIMUM_WIDTH 800
#define LABFY_DIALOG_MINIMUM_HEIGHT 600
#define LABFY_DIALOG_WORKAREA_FACTOR 0.90

typedef struct {
    double ratio;
    int minimum_start;
    int minimum_end;
} LabfyPanedGeometry;

LabfyDialogGeometry labfy_dialog_geometry_calculate(
    int workarea_width, int workarea_height)
{
    LabfyDialogGeometry geometry = {0};
    int usable_width = workarea_width > 0
        ? (workarea_width >= LABFY_DIALOG_MINIMUM_WIDTH
            ? workarea_width
            : MAX(1, (int) (
                workarea_width * LABFY_DIALOG_WORKAREA_FACTOR)))
        : LABFY_DIALOG_INITIAL_WIDTH;
    int usable_height = workarea_height > 0
        ? (workarea_height >= LABFY_DIALOG_MINIMUM_HEIGHT
            ? workarea_height
            : MAX(1, (int) (
                workarea_height * LABFY_DIALOG_WORKAREA_FACTOR)))
        : LABFY_DIALOG_INITIAL_HEIGHT;
    geometry.initial_width = MIN(LABFY_DIALOG_INITIAL_WIDTH, usable_width);
    geometry.initial_height = MIN(LABFY_DIALOG_INITIAL_HEIGHT, usable_height);
    geometry.minimum_width = workarea_width >= LABFY_DIALOG_MINIMUM_WIDTH
        ? LABFY_DIALOG_MINIMUM_WIDTH : usable_width;
    geometry.minimum_height = workarea_height >= LABFY_DIALOG_MINIMUM_HEIGHT
        ? LABFY_DIALOG_MINIMUM_HEIGHT : usable_height;
    geometry.minimum_width = MIN(
        geometry.minimum_width, geometry.initial_width);
    geometry.minimum_height = MIN(
        geometry.minimum_height, geometry.initial_height);
    return geometry;
}

static GdkMonitor *dialog_geometry_find_monitor(
    GtkWindow *window, GtkWindow *parent)
{
    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));
    GtkNative *native = parent != NULL
        ? gtk_widget_get_native(GTK_WIDGET(parent)) : NULL;
    GdkSurface *surface = native != NULL
        ? gtk_native_get_surface(native) : NULL;
    if (surface != NULL) {
        GdkMonitor *monitor =
            gdk_display_get_monitor_at_surface(display, surface);
        if (monitor != NULL) return g_object_ref(monitor);
    }
    GListModel *monitors = gdk_display_get_monitors(display);
    return monitors != NULL && g_list_model_get_n_items(monitors) > 0
        ? g_list_model_get_item(monitors, 0) : NULL;
}

void labfy_dialog_apply_standard_geometry(
    GtkWindow *window, GtkWindow *parent)
{
    GdkRectangle workarea = {0};
    GdkMonitor *monitor;
    LabfyDialogGeometry geometry;
    if (!GTK_IS_WINDOW(window)) return;
    monitor = dialog_geometry_find_monitor(window, parent);
    if (monitor != NULL) gdk_monitor_get_geometry(monitor, &workarea);
    geometry = labfy_dialog_geometry_calculate(
        workarea.width, workarea.height);
    gtk_window_set_resizable(window, TRUE);
    gtk_window_set_default_size(window,
        geometry.initial_width, geometry.initial_height);
    gtk_widget_set_size_request(GTK_WIDGET(window),
        geometry.minimum_width, geometry.minimum_height);
    g_clear_object(&monitor);
}

void labfy_dialog_prepare(
    GtkWindow *window,
    GtkWindow *parent,
    gboolean modal,
    gboolean destroy_with_parent)
{
    if (!GTK_IS_WINDOW(window)) return;
    gtk_window_set_transient_for(window,
        GTK_IS_WINDOW(parent) ? parent : NULL);
    gtk_window_set_modal(window, modal);
    gtk_window_set_destroy_with_parent(window,
        destroy_with_parent && GTK_IS_WINDOW(parent));
}

void labfy_dialog_present(GtkWindow *window)
{
    GtkWindow *parent;

    if (!GTK_IS_WINDOW(window)) return;
    parent = gtk_window_get_transient_for(window);
    labfy_dialog_apply_standard_geometry(window, parent);
    gtk_window_present(window);
}

static void paned_geometry_apply(
    GtkPaned *paned, LabfyPanedGeometry *geometry)
{
    int width = gtk_widget_get_width(GTK_WIDGET(paned));
    int position;
    if (geometry == NULL ||
        width < geometry->minimum_start + geometry->minimum_end) return;
    position = (int) (width * geometry->ratio);
    position = MAX(geometry->minimum_start, position);
    position = MIN(position, MAX(0, width - geometry->minimum_end));
    gtk_paned_set_position(paned, position);
}

static gboolean paned_geometry_idle(gpointer data)
{
    GtkPaned *paned = GTK_PANED(data);
    LabfyPanedGeometry *geometry = g_object_get_data(
        G_OBJECT(paned), "labfy-paned-initial-geometry");
    if (geometry == NULL) return G_SOURCE_REMOVE;
    if (gtk_widget_get_width(GTK_WIDGET(paned)) <
        geometry->minimum_start + geometry->minimum_end)
        return G_SOURCE_CONTINUE;
    paned_geometry_apply(paned, geometry);
    return G_SOURCE_REMOVE;
}

void labfy_paned_apply_initial_ratio(
    GtkPaned *paned, double ratio, int minimum_start, int minimum_end)
{
    LabfyPanedGeometry *geometry;
    if (!GTK_IS_PANED(paned)) return;
    if (g_object_get_data(
            G_OBJECT(paned), "labfy-paned-initial-geometry") != NULL)
        return;
    geometry = g_new0(LabfyPanedGeometry, 1);
    geometry->ratio = CLAMP(ratio, 0.1, 0.9);
    geometry->minimum_start = MAX(0, minimum_start);
    geometry->minimum_end = MAX(0, minimum_end);
    g_object_set_data_full(G_OBJECT(paned),
        "labfy-paned-initial-geometry", geometry, g_free);
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, paned_geometry_idle,
        g_object_ref(paned), g_object_unref);
}
