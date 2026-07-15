/******************************************************************************
 * @file schema.h
 * @brief Interface interne d'installation du schéma SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_SCHEMA_H
#define LABFY_INVESTIGATION_SCHEMA_H

#include <stdbool.h>

#include <sqlite3.h>

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
 * @param database Connexion SQLite ouverte.
 *
 * @return true si le schéma a été correctement installé, sinon false.
 */
bool schema_install_v1(
    sqlite3 *database
);

#endif
