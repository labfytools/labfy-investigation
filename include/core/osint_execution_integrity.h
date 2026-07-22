/******************************************************************************
 * @file osint_execution_integrity.h
 * @brief Calcul et vérification de l'intégrité des sorties OSINT.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_EXECUTION_INTEGRITY_H
#define LABFY_INVESTIGATION_OSINT_EXECUTION_INTEGRITY_H

#include <glib.h>

G_BEGIN_DECLS

/** @brief Résultat d'une vérification d'intégrité OSINT. */
typedef enum
{
    OSINT_EXECUTION_INTEGRITY_UNAVAILABLE = 0,
    OSINT_EXECUTION_INTEGRITY_INTACT,
    OSINT_EXECUTION_INTEGRITY_ALTERED
} OsintExecutionIntegrityStatus;

/**
 * @brief Calcule le SHA-256 de stdout, d'un séparateur nul et de stderr.
 * @param stdout_raw Sortie standard brute.
 * @param stderr_raw Sortie d'erreur brute.
 * @return Empreinte hexadécimale possédée, ou NULL en cas d'échec.
 */
char *osint_execution_integrity_calculate(GBytes *stdout_raw, GBytes *stderr_raw);

/**
 * @brief Compare les sorties brutes à une empreinte SHA-256 enregistrée.
 * @param stdout_raw Sortie standard brute.
 * @param stderr_raw Sortie d'erreur brute.
 * @param expected_sha256 Empreinte attendue.
 * @return État d'intégrité sans modifier les données fournies.
 */
OsintExecutionIntegrityStatus osint_execution_integrity_verify(
    GBytes *stdout_raw, GBytes *stderr_raw, const char *expected_sha256
);

G_END_DECLS
#endif
