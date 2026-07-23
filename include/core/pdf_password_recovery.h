/******************************************************************************
 * @file pdf_password_recovery.h
 * @brief Récupération locale du mot de passe d'une preuve PDF.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_PDF_PASSWORD_RECOVERY_H
#define LABFY_INVESTIGATION_PDF_PASSWORD_RECOVERY_H

#include "core/background_task.h"
#include "core/task_manager.h"

G_BEGIN_DECLS

/** @brief Méthode bornée utilisée par John the Ripper. */
typedef enum
{
    PDF_PASSWORD_RECOVERY_DICTIONARY,
    PDF_PASSWORD_RECOVERY_MASK,
    PDF_PASSWORD_RECOVERY_NUMERIC_RANGE
} PdfPasswordRecoveryMethod;

/** @brief Résultat opaque d'une récupération de mot de passe. */
typedef struct PdfPasswordRecoveryResult PdfPasswordRecoveryResult;

/**
 * @brief Lance la récupération dans le gestionnaire de tâches.
 * @param task_manager Gestionnaire recevant la tâche.
 * @param pdf_path Copie PDF à analyser.
 * @param output_directory Dossier de conservation du hash et du journal.
 * @param evidence_identifier UUID utilisé pour nommer les sorties.
 * @param method Méthode de récupération.
 * @param parameter Chemin du dictionnaire ou masque John.
 * @param completion_callback Callback exécuté sur le contexte principal.
 * @param completion_data Données transmises au callback.
 * @param completion_data_destroy Destructeur des données du callback.
 * @param error Emplacement facultatif pour l'erreur.
 * @return Tâche empruntée par le gestionnaire, ou NULL.
 */
BackgroundTask *pdf_password_recovery_start(TaskManager *task_manager,
    const char *pdf_path, const char *output_directory,
    const char *evidence_identifier, PdfPasswordRecoveryMethod method,
    const char *parameter, BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data, GDestroyNotify completion_data_destroy,
    GError **error);

/**
 * @brief Retourne le résultat d'une tâche terminée.
 * @param task Tâche créée par pdf_password_recovery_start().
 * @return Résultat emprunté, ou NULL.
 */
const PdfPasswordRecoveryResult *pdf_password_recovery_get_result(
    const BackgroundTask *task);

/**
 * @brief Indique si un mot de passe a été retrouvé.
 * @param result Résultat emprunté.
 * @return TRUE si un mot de passe est disponible.
 */
gboolean pdf_password_recovery_result_is_recovered(
    const PdfPasswordRecoveryResult *result);

/**
 * @brief Retourne le mot de passe retrouvé uniquement en mémoire.
 * @param result Résultat emprunté.
 * @return Mot de passe emprunté, ou NULL.
 */
const char *pdf_password_recovery_result_get_password(
    const PdfPasswordRecoveryResult *result);

G_END_DECLS

#endif
