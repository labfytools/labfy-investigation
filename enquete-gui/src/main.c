#include <gtk/gtk.h>
#include <sqlite3.h>

#include "app.h"
#include "database.h"

static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    GtkWidget *window = app_create_main_window(app);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
    sqlite3 *db = NULL;

    if (db_open(&db, "Enquete.sqlite") != 0) {
        return 1;
    }

    GtkApplication *app = gtk_application_new(
        "com.labfytools.enquete",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);
    db_close(db);

    return status;
}
