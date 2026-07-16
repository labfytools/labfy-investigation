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
 * @brief Enregistre une erreur dans une connexion Database.
 *
 * Le message est copié par la connexion.
 *
 * @param database Connexion Database.
 * @param error_code Code de l'erreur.
 * @param error_message Message descriptif.
 */
void database_error_set(
    Database *database,
    DatabaseErrorCode error_code,
    const char *error_message
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
