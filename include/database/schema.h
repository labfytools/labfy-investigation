/******************************************************************************
 * @file schema.h
 * @brief Interface interne d'installation du schéma SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_SCHEMA_H
#define LABFY_INVESTIGATION_SCHEMA_H

#include "database/database.h"

#include <stdbool.h>

/**
 * @brief Installe le schéma SQLite V1 dans une base ouverte.
 *
 * La connexion doit être valide et une transaction doit déjà être active.
 *
 * Cette fonction crée :
 *
 * - les tables ;
 * - les index ;
 * - les valeurs de référence.
 *
 * Elle ne réalise ni COMMIT ni ROLLBACK.
 *
 * @param database Connexion Database ouverte.
 *
 * @return true si le schéma a été correctement installé, sinon false.
 */
bool schema_install_v1(
    Database *database
);

/**
 * @brief Installe les modifications du schéma SQLite V2.
 *
 * La connexion doit être valide et une transaction doit déjà être active.
 *
 * Cette fonction :
 *
 * - étend la table preuves ;
 * - conserve les éventuelles lignes V1 ;
 * - ajoute les contraintes nécessaires aux nouvelles preuves ;
 * - ajoute les index V2.
 *
 * Elle ne modifie pas elle-même la version enregistrée dans metadata.
 * Elle ne réalise ni COMMIT ni ROLLBACK.
 *
 * @param database Connexion Database ouverte.
 *
 * @return true si la migration V2 a été appliquée, sinon false.
 */
bool schema_install_v2(
    Database *database
);

/**
 * @brief Installe la migration de provenance OSINT du schéma V3.
 *
 * La connexion doit être valide et une transaction doit déjà être active.
 * La fonction ne réalise ni COMMIT ni ROLLBACK.
 *
 * @param database Connexion Database ouverte.
 *
 * @return true si la migration V3 a été appliquée, sinon false.
 */
bool schema_install_v3(
    Database *database
);

/**
 * @brief Installe la migration des comptes sociaux du schéma V4.
 *
 * La connexion doit être valide et une transaction doit déjà être active.
 * La fonction ne réalise ni COMMIT ni ROLLBACK.
 *
 * @param database Connexion Database ouverte.
 * @return true si la migration V4 a été appliquée, sinon false.
 */
bool schema_install_v4(
    Database *database
);

/**
 * @brief Garantit la présence des extensions du schéma courant V2.
 *
 * La connexion doit être valide et une transaction doit déjà être active.
 *
 * Cette fonction applique uniquement des opérations idempotentes destinées
 * aux nouvelles bases comme aux bases V2 déjà existantes.
 *
 * Elle ne modifie pas la version enregistrée dans metadata.
 * Elle ne réalise ni COMMIT ni ROLLBACK.
 *
 * @param database Connexion Database ouverte.
 *
 * @return true si les extensions sont présentes, sinon false.
 */
bool schema_ensure_current(
    Database *database
);

#endif
