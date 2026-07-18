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

#endif
