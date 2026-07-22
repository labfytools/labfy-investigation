/******************************************************************************
 * @file osint_execution_record.h
 * @brief Modèle immuable d'une exécution OSINT persistée.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_EXECUTION_RECORD_H
#define LABFY_INVESTIGATION_OSINT_EXECUTION_RECORD_H

#include <gio/gio.h>

G_BEGIN_DECLS

/** @brief Exécution OSINT opaque. */
typedef struct OsintExecutionRecord OsintExecutionRecord;

/** @brief Crée une exécution OSINT en copiant toutes ses données. */
OsintExecutionRecord *osint_execution_record_new(
    const char *identifier,
    const char *tool_identifier,
    const char *tool_version,
    const char *action_identifier,
    const char *selection_identifier,
    const char *selection_kind,
    const char *target_value,
    const char *arguments,
    const char *started_at,
    const char *finished_at,
    gboolean has_exit_code,
    gint exit_code,
    const char *final_state,
    GBytes *stdout_raw,
    GBytes *stderr_raw,
    const char *output_sha256,
    GError **error
);

/** @brief Libère un modèle, ou accepte NULL. */
void osint_execution_record_free(OsintExecutionRecord *record);

/** @brief Retourne l'UUID emprunté. */
const char *osint_execution_record_get_identifier(const OsintExecutionRecord *record);
/** @brief Retourne l'identifiant d'outil emprunté. */
const char *osint_execution_record_get_tool_identifier(const OsintExecutionRecord *record);
/** @brief Retourne la version d'outil empruntée, ou NULL. */
const char *osint_execution_record_get_tool_version(const OsintExecutionRecord *record);
/** @brief Retourne l'identifiant d'action emprunté. */
const char *osint_execution_record_get_action_identifier(const OsintExecutionRecord *record);
/** @brief Retourne l'UUID de sélection emprunté. */
const char *osint_execution_record_get_selection_identifier(const OsintExecutionRecord *record);
/** @brief Retourne la nature de sélection empruntée. */
const char *osint_execution_record_get_selection_kind(const OsintExecutionRecord *record);
/** @brief Retourne la cible empruntée. */
const char *osint_execution_record_get_target_value(const OsintExecutionRecord *record);
/** @brief Retourne les arguments déterministes empruntés. */
const char *osint_execution_record_get_arguments(const OsintExecutionRecord *record);
/** @brief Retourne la date UTC de début empruntée. */
const char *osint_execution_record_get_started_at(const OsintExecutionRecord *record);
/** @brief Retourne la date UTC de fin empruntée. */
const char *osint_execution_record_get_finished_at(const OsintExecutionRecord *record);
/** @brief Indique si un code de sortie est disponible. */
gboolean osint_execution_record_has_exit_code(const OsintExecutionRecord *record);
/** @brief Retourne le code de sortie, ou -1 s'il est absent. */
gint osint_execution_record_get_exit_code(const OsintExecutionRecord *record);
/** @brief Retourne l'état final emprunté. */
const char *osint_execution_record_get_final_state(const OsintExecutionRecord *record);
/** @brief Retourne une référence sur stdout brut. */
GBytes *osint_execution_record_ref_stdout(const OsintExecutionRecord *record);
/** @brief Retourne une référence sur stderr brut. */
GBytes *osint_execution_record_ref_stderr(const OsintExecutionRecord *record);
/** @brief Retourne l'empreinte SHA-256 empruntée. */
const char *osint_execution_record_get_output_sha256(const OsintExecutionRecord *record);

G_END_DECLS
#endif
