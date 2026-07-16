/******************************************************************************
 * @file investigation_session.h
 * @brief Interface publique d'une session d'enquête ouverte.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_SESSION_H
#define LABFY_INVESTIGATION_INVESTIGATION_SESSION_H

#include <glib.h>

#include "core/investigation_project.h"
#include "database/database.h"
#include "models/investigation_record.h"

/**
 * @brief Session opaque représentant une enquête ouverte.
 *
 * Une session possède :
 *
 * - le contexte de fichiers InvestigationProject ;
 * - la connexion Database ;
 * - les informations persistées InvestigationRecord.
 */
typedef struct InvestigationSession InvestigationSession;

/**
 * @brief Codes d'erreur produits lors de l'ouverture d'une session.
 */
typedef enum
{
    INVESTIGATION_SESSION_ERROR_INVALID_ARGUMENT,
    INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND,
    INVESTIGATION_SESSION_ERROR_DATABASE_NOT_FOUND,
    INVESTIGATION_SESSION_ERROR_PROJECT,
    INVESTIGATION_SESSION_ERROR_DATABASE,
    INVESTIGATION_SESSION_ERROR_RECORD,
    INVESTIGATION_SESSION_ERROR_ROOT_MISMATCH,
    INVESTIGATION_SESSION_ERROR_MEMORY
} InvestigationSessionError;

/**
 * @brief Domaine d'erreur GLib du module InvestigationSession.
 */
#define INVESTIGATION_SESSION_ERROR \
    investigation_session_error_quark()

/**
 * @brief Retourne le domaine d'erreur du module.
 *
 * @return Le GQuark associé aux erreurs InvestigationSession.
 */
GQuark investigation_session_error_quark(void);

/**
 * @brief Ouvre une enquête existante.
 *
 * La fonction :
 *
 * - vérifie que le chemin racine représente un dossier ;
 * - construit un InvestigationProject ;
 * - vérifie la présence du fichier SQLite ;
 * - ouvre la connexion Database ;
 * - charge le record persistant ;
 * - vérifie la cohérence du chemin racine.
 *
 * Cette fonction ne crée aucune enquête et ne modifie pas la base.
 *
 * En cas de succès, la session devient propriétaire du projet,
 * de la connexion Database et du record.
 *
 * Le paramètre error peut être NULL.
 *
 * @param investigation_root_path Chemin racine de l'enquête.
 * @param error Adresse recevant une éventuelle erreur GLib.
 *
 * @return Une nouvelle session, ou NULL en cas d'échec.
 */
InvestigationSession *investigation_session_open(
    const char *investigation_root_path,
    GError **error
);

/**
 * @brief Ferme une session d'enquête.
 *
 * Cette fonction libère :
 *
 * - le record ;
 * - la connexion Database ;
 * - le contexte InvestigationProject ;
 * - la session.
 *
 * Cette fonction accepte NULL.
 *
 * @param session Session à fermer.
 */
void investigation_session_close(
    InvestigationSession *session
);

/**
 * @brief Retourne le contexte de fichiers de la session.
 *
 * Le pointeur retourné appartient à la session et ne doit pas être libéré.
 *
 * @param session Session d'enquête.
 *
 * @return Le projet associé, ou NULL si session vaut NULL.
 */
const InvestigationProject *investigation_session_get_project(
    const InvestigationSession *session
);

/**
 * @brief Retourne les informations persistées de l'enquête.
 *
 * Le pointeur retourné appartient à la session et ne doit pas être libéré.
 *
 * @param session Session d'enquête.
 *
 * @return Le record associé, ou NULL si session vaut NULL.
 */
const InvestigationRecord *investigation_session_get_record(
    const InvestigationSession *session
);

/**
 * @brief Retourne la connexion Database de la session.
 *
 * Le pointeur retourné appartient à la session et ne doit pas être fermé
 * par le code appelant.
 *
 * La connexion reste modifiable afin d'être utilisée par les futurs DAO.
 *
 * @param session Session d'enquête.
 *
 * @return La connexion associée, ou NULL si session vaut NULL.
 */
Database *investigation_session_get_database(
    InvestigationSession *session
);

#endif
