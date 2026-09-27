/******************************************************************************
 * @file database.h
 * @brief API principale de la couche Database.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_DATABASE_H
#define LABFY_INVESTIGATION_DATABASE_H

#include <stdbool.h>
#include <glib.h>

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
 * @brief Ouvre une base runtime existante en lecture seule effective.
 *
 * CONTRACT: cette ouverture utilise SQLITE_OPEN_READONLY, ne crée jamais un
 * fichier absent, n'exécute aucune migration et refuse toute version autre que
 * la version runtime prise en charge. PRAGMA query_only renforce la frontière
 * pour les requêtes préparées par les DAO.
 *
 * La chaîne de chemin est copiée. L'instance retournée appartient à l'appelant
 * et doit être libérée avec database_close(). En cas d'échec, aucune connexion
 * n'est retournée et error reçoit un diagnostic structuré si fourni.
 *
 * @param database_path Chemin d'une base SQLite existante.
 * @param error Emplacement facultatif recevant une erreur G_IO_ERROR.
 *
 * @return Nouvelle connexion read-only, ou NULL.
 */
Database *database_open_read_only(
    const char *database_path,
    GError **error
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
 * @brief Met à jour une base ouverte vers la dernière version du schéma.
 *
 * La fonction :
 *
 * - lit metadata.schema_version ;
 * - applique chaque migration manquante dans une transaction ;
 * - met à jour la version uniquement après une migration réussie ;
 * - ne modifie rien lorsque la base est déjà à jour.
 *
 * La fonction refuse une migration lorsqu’une transaction est déjà active.
 *
 * @param database Connexion Database ouverte.
 *
 * @return true si la base est à jour, sinon false.
 */
bool database_migrate_to_latest(
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
