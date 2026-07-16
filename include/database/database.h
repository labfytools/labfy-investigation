/******************************************************************************
 * @file database.h
 * @brief API principale de la couche Database.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_DATABASE_H
#define LABFY_INVESTIGATION_DATABASE_H

#include <stdbool.h>

/**
 * @brief Contexte opaque représentant une connexion SQLite.
 */
typedef struct Database Database;

/**
 * @brief Ouvre une base SQLite existante ou à créer.
 *
 * La fonction :
 *
 * - valide le chemin ;
 * - ouvre la connexion SQLite ;
 * - active les clés étrangères ;
 * - conserve une copie du chemin.
 *
 * @param database_path Chemin du fichier SQLite.
 *
 * @return Une nouvelle instance de Database, ou NULL en cas d'échec.
 */
Database *database_open(
    const char *database_path
);

/**
 * @brief Ferme une base SQLite et libère ses ressources.
 *
 * Cette fonction accepte NULL.
 *
 * @param database Instance à fermer.
 */
void database_close(
    Database *database
);

/**
 * @brief Initialise la base SQLite d'une nouvelle enquête.
 *
 * Cette fonction conserve temporairement son rôle actuel pendant le
 * refactoring du ticket #025.
 *
 * @param database_path Chemin complet du fichier Enquete.sqlite.
 * @param investigation_name Nom de l'enquête.
 * @param investigation_root_path Chemin racine de l'enquête.
 *
 * @return true si l'initialisation réussit, sinon false.
 */
bool database_initialize(
    const char *database_path,
    const char *investigation_name,
    const char *investigation_root_path
);

#endif
