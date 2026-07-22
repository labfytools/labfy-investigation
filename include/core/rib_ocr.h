/******************************************************************************
 * @file rib_ocr.h
 * @brief Exécution locale de Tesseract pour les preuves de type RIB.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RIB_OCR_H
#define LABFY_INVESTIGATION_RIB_OCR_H
#include <glib.h>
G_BEGIN_DECLS
/** @brief Exécute Tesseract localement et retourne le texte UTF-8 possédé. */
gboolean rib_ocr_extract_text(const char *image_path, char **out_text,
    char **out_version, GError **error);
G_END_DECLS
#endif
