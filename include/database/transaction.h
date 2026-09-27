/******************************************************************************
 * @file transaction.h
 * @brief Gestion des transactions SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TRANSACTION_H
#define LABFY_INVESTIGATION_TRANSACTION_H

#include "database/database.h"

#include <stdbool.h>

/**
 * @brief Indique si une transaction est actuellement active.
 *
 * Cette fonction ne modifie ni la connexion ni son erreur courante.
 *
 * @param database Connexion à inspecter.
 *
 * @return true lorsqu’une transaction est active, sinon false.
 *         Une connexion NULL retourne false.
 */
bool database_transaction_is_active(
    const Database *database
);

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
 * @brief Démarre une transaction différée destinée à un snapshot de lecture.
 *
 * CONTRACT: contrairement à database_transaction_begin(), cette fonction ne
 * demande aucun verrou d'écriture. La première lecture fixe donc un snapshot
 * SQLite cohérent, y compris sur une connexion ouverte read-only. Une seule
 * transaction peut être active sur l'instance.
 *
 * @param database Connexion à la base.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_transaction_begin_read_only(
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
