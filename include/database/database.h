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
 * La fonction crée ou ouvre le fichier SQLite indiqué, installe le schéma
 * courant, puis enregistre les métadonnées de l'enquête.
 *
 * Aucun handle SQLite n'est exposé au code appelant.
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
