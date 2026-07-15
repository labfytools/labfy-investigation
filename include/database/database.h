/******************************************************************************
 * @file database.h
 * @brief Interface publique d'initialisation de la base SQLite d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_DATABASE_H
#define LABFY_INVESTIGATION_DATABASE_H

#include <stdbool.h>

/**
 * @brief Initialise la base SQLite d'une enquête.
 *
 * La fonction crée ou ouvre le fichier SQLite indiqué, puis initialise
 * transactionnellement le schéma minimal de l'enquête.
 *
 * Les informations suivantes sont enregistrées :
 *
 * - version du schéma ;
 * - nom de l'application ;
 * - date de création UTC ;
 * - UUID de l'enquête ;
 * - nom de l'enquête ;
 * - chemin racine de l'enquête.
 *
 * Aucun handle sqlite3 n'est exposé au code appelant.
 *
 * @param database_path
 *        Chemin complet du fichier Enquete.sqlite.
 *
 * @param investigation_name
 *        Nom de l'enquête.
 *
 * @param investigation_root_path
 *        Chemin complet du dossier racine de l'enquête.
 *
 * @return true si la base a été correctement initialisée,
 *         sinon false.
 */
bool database_initialize(
    const char *database_path,
    const char *investigation_name,
    const char *investigation_root_path
);

#endif
