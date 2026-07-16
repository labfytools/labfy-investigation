/******************************************************************************
 * @file database_internal.h
 * @brief API interne de la couche Database.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_DATABASE_INTERNAL_H
#define LABFY_INVESTIGATION_DATABASE_INTERNAL_H

#include "database/database.h"
#include "database/error.h"

#include <sqlite3.h>
#include <stdbool.h>

/**
 * @brief Retourne le handle SQLite interne.
 *
 * Cette fonction est réservée aux modules de src/database/.
 *
 * @param database Connexion Database.
 *
 * @return Handle SQLite, ou NULL si l'instance est invalide.
 */
sqlite3 *database_get_handle(
    Database *database
);

/**
 * @brief Indique si une transaction est active.
 */
bool database_get_transaction_active(
    const Database *database
);

/**
 * @brief Modifie l'état interne de la transaction.
 */
void database_set_transaction_active(
    Database *database,
    bool transaction_active
);

/**
 * @brief Enregistre une erreur dans une connexion Database.
 */
void database_set_error(
    Database *database,
    DatabaseErrorCode error_code,
    const char *error_message
);

/**
 * @brief Retourne le code d'erreur interne.
 */
DatabaseErrorCode database_get_error_code_internal(
    const Database *database
);

/**
 * @brief Retourne le message d'erreur interne.
 */
const char *database_get_error_message_internal(
    const Database *database
);

/**
 * @brief Efface l'erreur interne.
 */
void database_clear_error_internal(
    Database *database
);

#endif
