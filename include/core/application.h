/******************************************************************************
 * @file application.h
 * @brief Interface publique du cycle de vie de l'application.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_APPLICATION_H
#define LABFY_INVESTIGATION_APPLICATION_H

/**
 * @brief Représentation opaque d'une session d'enquête.
 */
typedef struct InvestigationSession InvestigationSession;

/**
 * @brief Représentation opaque de l'application.
 *
 * Les détails GTK sont volontairement masqués au reste du programme.
 */
typedef struct Application Application;

/**
 * @brief Crée et initialise une application Labfy Investigation.
 *
 * @return Une nouvelle application, ou NULL en cas d'échec.
 */
Application *application_new(void);

/**
 * @brief Lance la boucle principale de l'application.
 *
 * @param application Application à exécuter.
 * @param argc Nombre d'arguments de la ligne de commande.
 * @param argv Tableau des arguments de la ligne de commande.
 *
 * @return Code de sortie retourné par GTK.
 */
int application_run(
    Application *application,
    int argc,
    char **argv
);

/**
 * @brief Retourne la session d'enquête actuellement ouverte.
 *
 * Le pointeur retourné appartient à l'application.
 * Le code appelant ne doit pas fermer cette session.
 *
 * @param application Application concernée.
 *
 * @return La session active, ou NULL si aucune enquête n'est ouverte
 *         ou si application vaut NULL.
 */
const InvestigationSession *application_get_session(
    const Application *application
);

/**
 * @brief Libère les ressources possédées par l'application.
 *
 * Cette fonction accepte NULL.
 *
 * @param application Application à libérer.
 */
void application_free(
    Application *application
);

#endif
