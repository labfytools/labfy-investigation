#include "views/dialog_geometry.h"

static void test_calculations(void)
{
    LabfyDialogGeometry geometry =
        labfy_dialog_geometry_calculate(1920, 1080);
    g_assert_cmpint(geometry.initial_width, ==, 1200);
    g_assert_cmpint(geometry.initial_height, ==, 800);
    g_assert_cmpint(geometry.minimum_width, ==, 800);
    g_assert_cmpint(geometry.minimum_height, ==, 600);
    geometry = labfy_dialog_geometry_calculate(800, 600);
    g_assert_cmpint(geometry.initial_width, ==, 800);
    g_assert_cmpint(geometry.initial_height, ==, 600);
    g_assert_cmpint(geometry.minimum_width, ==, 800);
    g_assert_cmpint(geometry.minimum_height, ==, 600);
    geometry = labfy_dialog_geometry_calculate(760, 560);
    g_assert_cmpint(geometry.initial_width, ==, 684);
    g_assert_cmpint(geometry.initial_height, ==, 504);
    g_assert_cmpint(geometry.minimum_width, ==, 684);
    g_assert_cmpint(geometry.minimum_height, ==, 504);
    geometry = labfy_dialog_geometry_calculate(0, 0);
    g_assert_cmpint(geometry.initial_width, ==, 1200);
    g_assert_cmpint(geometry.initial_height, ==, 800);
}

static gboolean frame_seen(GtkWidget *widget, GdkFrameClock *clock,
    gpointer data)
{
    gboolean *seen = data;
    (void) widget;
    (void) clock;
    *seen = TRUE;
    return G_SOURCE_REMOVE;
}

static void wait_for_frame(GtkWidget *widget)
{
    gboolean seen = FALSE;
    gtk_widget_add_tick_callback(widget, frame_seen, &seen, NULL);
    while (!seen)
        g_main_context_iteration(NULL, TRUE);
}

static void test_paned_applied_once(void)
{
    GtkWindow *window = GTK_WINDOW(gtk_window_new());
    GtkPaned *paned = GTK_PANED(
        gtk_paned_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_paned_set_start_child(paned, gtk_label_new("Formulaire"));
    gtk_paned_set_end_child(paned, gtk_label_new("Aperçu"));
    gtk_window_set_default_size(window, 900, 600);
    gtk_window_set_child(window, GTK_WIDGET(paned));
    labfy_paned_apply_initial_ratio(paned, 2.0 / 3.0, 320, 180);
    gtk_window_present(window);
    for (guint frame = 0; frame < 6; frame++)
        wait_for_frame(GTK_WIDGET(window));
    int expected = (gtk_widget_get_width(GTK_WIDGET(paned)) * 2) / 3;
    g_assert_cmpint(ABS(gtk_paned_get_position(paned) - expected), <=, 2);
    gtk_paned_set_position(paned, 450);
    gtk_window_set_default_size(window, 1000, 700);
    wait_for_frame(GTK_WIDGET(window));
    g_assert_cmpint(gtk_paned_get_position(paned), ==, 450);
    labfy_paned_apply_initial_ratio(paned, 0.5, 100, 100);
    g_assert_cmpint(gtk_paned_get_position(paned), ==, 450);
    gtk_window_destroy(window);
}

static void test_parent_and_presentation(void)
{
    for (guint cycle = 0; cycle < 20; cycle++) {
        GtkWindow *parent = GTK_WINDOW(gtk_window_new());
        GtkWindow *dialog = GTK_WINDOW(gtk_window_new());
        GtkWindow *nested = GTK_WINDOW(gtk_window_new());
        GtkWindow *dialog_observer = dialog;
        GtkWindow *nested_observer = nested;
        int width = 0;
        int height = 0;

        gtk_window_present(parent);
        labfy_dialog_prepare(dialog, parent, TRUE, TRUE);
        g_assert_true(gtk_window_get_transient_for(dialog) == parent);
        g_assert_true(gtk_window_get_modal(dialog));
        g_assert_true(gtk_window_get_destroy_with_parent(dialog));
        labfy_dialog_present(dialog);
        gtk_window_get_default_size(dialog, &width, &height);
        g_assert_cmpint(width, >, 0);
        g_assert_cmpint(height, >, 0);
        g_assert_true(gtk_widget_get_visible(GTK_WIDGET(dialog)));

        labfy_dialog_prepare(nested, dialog, TRUE, TRUE);
        g_assert_true(gtk_window_get_transient_for(nested) == dialog);
        labfy_dialog_present(nested);
        g_assert_true(gtk_widget_get_visible(GTK_WIDGET(nested)));

        g_object_add_weak_pointer(
            G_OBJECT(dialog), (gpointer *) &dialog_observer);
        g_object_add_weak_pointer(
            G_OBJECT(nested), (gpointer *) &nested_observer);
        gtk_window_destroy(parent);
        while (g_main_context_iteration(NULL, FALSE));
        g_assert_null(dialog_observer);
        g_assert_null(nested_observer);
    }
}

int main(int argc, char **argv)
{
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/dialog-geometry/calculations", test_calculations);
    g_test_add_func("/dialog-geometry/paned-applied-once",
        test_paned_applied_once);
    g_test_add_func("/dialog-geometry/parent-and-presentation",
        test_parent_and_presentation);
    return g_test_run();
}
