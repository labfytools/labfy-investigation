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
 * Cette fonction :
 *
 * - valide les paramètres ;
 * - ouvre la connexion Database ;
 * - démarre une transaction ;
 * - installe le schéma SQLite V1 ;
 * - insère les métadonnées obligatoires ;
 * - insère l'enquête courante ;
 * - valide la transaction ;
 * - ferme la connexion avant de retourner.
 *
 * Tout échec survenant après le début de la transaction provoque
 * l'annulation des modifications.
 *
 * La fonction ne crée pas les dossiers parents du fichier SQLite.
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
