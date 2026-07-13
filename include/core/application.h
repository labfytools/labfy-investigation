/******************************************************************************
 * @file application.h
 * @brief Interface publique du cycle de vie de l'application.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_APPLICATION_H
#define LABFY_INVESTIGATION_APPLICATION_H

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
int application_run(Application *application, int argc, char **argv);

/**
 * @brief Libère les ressources possédées par l'application.
 *
 * Cette fonction accepte NULL.
 *
 * @param application Application à libérer.
 */
void application_free(Application *application);

#endif
