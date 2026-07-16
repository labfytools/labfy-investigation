/******************************************************************************
 * @file error.h
 * @brief Gestion des erreurs de la couche Database.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_DATABASE_ERROR_H
#define LABFY_INVESTIGATION_DATABASE_ERROR_H

#include "database/database.h"

/**
 * @brief Catégories d'erreurs de la couche Database.
 */
typedef enum
{
    DATABASE_ERROR_NONE = 0,
    DATABASE_ERROR_INVALID_ARGUMENT,
    DATABASE_ERROR_MEMORY,
    DATABASE_ERROR_SQLITE,
    DATABASE_ERROR_INVALID_STATE
} DatabaseErrorCode;

/**
 * @brief Retourne le code de la dernière erreur.
 *
 * @param database Connexion Database.
 *
 * @return Code d'erreur, ou DATABASE_ERROR_NONE.
 */
DatabaseErrorCode database_error_get_code(
    const Database *database
);

/**
 * @brief Retourne le message de la dernière erreur.
 *
 * Le pointeur retourné appartient à Database et ne doit pas être libéré.
 *
 * @param database Connexion Database.
 *
 * @return Message d'erreur, ou NULL.
 */
const char *database_error_get_message(
    const Database *database
);

/**
 * @brief Efface la dernière erreur enregistrée.
 *
 * @param database Connexion Database.
 */
void database_error_clear(
    Database *database
);

#endif
