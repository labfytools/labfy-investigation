/******************************************************************************
 * @file iban_analyzer.h
 * @brief Normalisation, validation et extraction locale d'IBAN.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_IBAN_ANALYZER_H
#define LABFY_INVESTIGATION_IBAN_ANALYZER_H
#include <glib.h>
G_BEGIN_DECLS
/** @brief Normalise un IBAN en majuscules sans séparateurs. */
char *iban_analyzer_normalize(const char *text);
/** @brief Vérifie la structure et la clé de contrôle modulo 97. */
gboolean iban_analyzer_validate(const char *iban);
/**
 * @brief Extrait les IBAN valides d'un texte OCR.
 * @return Tableau possédé de chaînes uniques normalisées.
 */
GPtrArray *iban_analyzer_extract(const char *ocr_text);
G_END_DECLS
#endif
