/******************************************************************************
 * @file metadata_analysis_dialog.h
 * @brief Fenêtre de consultation des métadonnées extraites.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_METADATA_ANALYSIS_DIALOG_H
#define LABFY_INVESTIGATION_METADATA_ANALYSIS_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Affiche une synthèse ExifTool dans une fenêtre défilable.
 * @param parent Fenêtre parente facultative.
 * @param summary Métadonnées lisibles empruntées.
 * @param json_path Chemin emprunté de la sortie JSON conservée.
 * @param version Version empruntée d'ExifTool.
 */
void metadata_analysis_dialog_present(GtkWindow *parent, const char *summary,
    const char *json_path, const char *version);

G_END_DECLS

#endif
