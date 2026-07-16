/******************************************************************************
 * @file transaction.h
 * @brief Gestion des transactions SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TRANSACTION_H
#define LABFY_INVESTIGATION_TRANSACTION_H

#include "database/database.h"

#include <stdbool.h>

/**
 * @brief Démarre une transaction d'écriture immédiate.
 *
 * Une seule transaction peut être active sur une instance Database.
 *
 * @param database Connexion à la base.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_transaction_begin(
    Database *database
);

/**
 * @brief Valide la transaction active.
 *
 * @param database Connexion à la base.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_transaction_commit(
    Database *database
);

/**
 * @brief Annule la transaction active.
 *
 * @param database Connexion à la base.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_transaction_rollback(
    Database *database
);

#endif
