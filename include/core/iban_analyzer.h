/******************************************************************************
 * @file iban_analyzer.h
 * @brief Normalisation, validation et extraction locale d'IBAN.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_IBAN_ANALYZER_H
#define LABFY_INVESTIGATION_IBAN_ANALYZER_H
#include <glib.h>
G_BEGIN_DECLS

typedef enum
{
    IBAN_ANALYZER_RESULT_VALID,
    IBAN_ANALYZER_RESULT_INVALID,
    IBAN_ANALYZER_RESULT_UNSUPPORTED_COUNTRY
} IbanAnalyzerResult;
/** @brief Normalise un IBAN en majuscules sans séparateurs. */
char *iban_analyzer_normalize(const char *text);
/** Normalise uniquement espaces ASCII, tabulations, CR/LF et tirets. */
char *iban_analyzer_normalize_canonical(const char *text);
/**
 * Vérifie format, longueur nationale prise en charge et MOD-97.
 *
 * La table volontairement partielle de cette tranche couvre BE, DE, ES, FR,
 * GB, IT, LU, NL et PT. Un code pays absent ne peut jamais être déclaré
 * valide : iban_analyzer_validate_canonical_result() retourne alors
 * IBAN_ANALYZER_RESULT_UNSUPPORTED_COUNTRY.
 */
gboolean iban_analyzer_validate_canonical(const char *iban);
IbanAnalyzerResult iban_analyzer_validate_canonical_result(const char *iban);
/** @brief Vérifie la structure et la clé de contrôle modulo 97. */
gboolean iban_analyzer_validate(const char *iban);
/**
 * @brief Extrait les IBAN valides d'un texte OCR.
 * @return Tableau possédé de chaînes uniques normalisées.
 */
GPtrArray *iban_analyzer_extract(const char *ocr_text);
G_END_DECLS
#endif
