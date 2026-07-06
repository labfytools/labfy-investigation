#include "app.h"

GtkWidget *app_create_main_window(GtkApplication *app)
{
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *sidebar;
    GtkWidget *content;
    GtkWidget *label;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Enquête OSINT");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 650);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), box);

    sidebar = gtk_list_box_new();
    gtk_widget_set_size_request(sidebar, 220, -1);

    gtk_list_box_append(GTK_LIST_BOX(sidebar), gtk_label_new("Preuves"));
    gtk_list_box_append(GTK_LIST_BOX(sidebar), gtk_label_new("Entités"));
    gtk_list_box_append(GTK_LIST_BOX(sidebar), gtk_label_new("Chronologie"));
    gtk_list_box_append(GTK_LIST_BOX(sidebar), gtk_label_new("Recherches"));
    gtk_list_box_append(GTK_LIST_BOX(sidebar), gtk_label_new("Hypothèses"));
    gtk_list_box_append(GTK_LIST_BOX(sidebar), gtk_label_new("Sources"));
    gtk_list_box_append(GTK_LIST_BOX(sidebar), gtk_label_new("Rapport"));

    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(content, 20);
    gtk_widget_set_margin_bottom(content, 20);
    gtk_widget_set_margin_start(content, 20);
    gtk_widget_set_margin_end(content, 20);

    label = gtk_label_new("Tableau de bord de l'enquête");
    gtk_box_append(GTK_BOX(content), label);

    gtk_box_append(GTK_BOX(box), sidebar);
    gtk_box_append(GTK_BOX(box), content);

    return window;
}
