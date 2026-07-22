/******************************************************************************
 * @file eml_analyzer.h
 * @brief Analyse locale et non destructive des en-têtes d'un fichier EML.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EML_ANALYZER_H
#define LABFY_INVESTIGATION_EML_ANALYZER_H
#include <glib.h>
G_BEGIN_DECLS
/** @brief Résultat opaque d'une analyse d'en-têtes EML. */
typedef struct EmlAnalysis EmlAnalysis;
/**
 * @brief Analyse uniquement les en-têtes d'un fichier EML local.
 * @param file_path Chemin du fichier original en lecture seule.
 * @param error Destination facultative d'une erreur.
 * @return Nouvelle analyse, ou NULL.
 */
EmlAnalysis *eml_analyzer_analyze_file(const char *file_path, GError **error);
/** @brief Libère une analyse. */
void eml_analysis_free(EmlAnalysis *analysis);
/** @brief Retourne la première valeur d'un en-tête, ou NULL. */
const char *eml_analysis_get_first_header(const EmlAnalysis *analysis,
    const char *header_name);
/** @brief Retourne les valeurs ordonnées d'un en-tête, ou NULL. */
const GPtrArray *eml_analysis_get_header_values(const EmlAnalysis *analysis,
    const char *header_name);
/** @brief Retourne les adresses email uniques extraites. */
const GPtrArray *eml_analysis_get_email_addresses(const EmlAnalysis *analysis);
/** @brief Retourne les domaines uniques extraits. */
const GPtrArray *eml_analysis_get_domains(const EmlAnalysis *analysis);
/** @brief Retourne les adresses IP uniques extraites. */
const GPtrArray *eml_analysis_get_ip_addresses(const EmlAnalysis *analysis);
/** @brief Retourne les IP présentes dans la partie `from` des Received. */
const GPtrArray *eml_analysis_get_sender_ip_addresses(
    const EmlAnalysis *analysis);
/** @brief Retourne les IP présentes dans la partie `by` des Received. */
const GPtrArray *eml_analysis_get_destination_ip_addresses(
    const EmlAnalysis *analysis);
/** @brief Retourne une copie UTF-8 des en-têtes bruts. */
const char *eml_analysis_get_raw_headers(const EmlAnalysis *analysis);
G_END_DECLS
#endif
