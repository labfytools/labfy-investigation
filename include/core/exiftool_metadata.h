/******************************************************************************
 * @file exiftool_metadata.h
 * @brief Extraction locale des métadonnées techniques d'une preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EXIFTOOL_METADATA_H
#define LABFY_INVESTIGATION_EXIFTOOL_METADATA_H

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * @brief Extrait une synthèse lisible et la sortie JSON brute avec ExifTool.
 * @param file_path Chemin du fichier de travail à analyser.
 * @param summary Emplacement recevant la synthèse nouvellement allouée.
 * @param json Emplacement recevant le JSON nouvellement alloué.
 * @param version Emplacement facultatif recevant la version de l'outil.
 * @param error Emplacement facultatif pour l'erreur.
 * @return TRUE lorsque les deux extractions ont réussi.
 */
gboolean exiftool_metadata_extract(const char *file_path, char **summary,
    char **json, char **version, GError **error);

G_END_DECLS

#endif
