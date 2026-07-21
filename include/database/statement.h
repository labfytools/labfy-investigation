/******************************************************************************
 * @file statement.h
 * @brief API des requêtes préparées de la couche Database.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_STATEMENT_H
#define LABFY_INVESTIGATION_STATEMENT_H

#include "database/database.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Résultat de l'exécution d'une requête préparée.
 */
typedef enum
{
    DATABASE_STATEMENT_STEP_ERROR = 0,
    DATABASE_STATEMENT_STEP_ROW,
    DATABASE_STATEMENT_STEP_DONE
} DatabaseStatementStepResult;

/**
 * @brief Requête SQLite préparée encapsulée.
 */
typedef struct DatabaseStatement DatabaseStatement;

/**
 * @brief Prépare une requête SQL.
 *
 * La requête doit contenir une seule instruction SQL.
 *
 * @param database Connexion ouverte.
 * @param sql      Requête SQL à préparer.
 *
 * @return Une nouvelle requête préparée, ou NULL en cas d'échec.
 */
DatabaseStatement *database_statement_prepare(
    Database *database,
    const char *sql
);

/**
 * @brief Exécute une étape d'une requête préparée.
 *
 * Pour une requête SELECT, DATABASE_STATEMENT_STEP_ROW indique
 * qu'une ligne est disponible.
 *
 * DATABASE_STATEMENT_STEP_DONE indique que l'exécution est terminée.
 *
 * @param statement Requête préparée.
 *
 * @return Résultat de l'étape.
 */
DatabaseStatementStepResult database_statement_step(
    DatabaseStatement *statement
);

/**
 * @brief Réinitialise une requête préparée afin de pouvoir la réexécuter.
 *
 * Les paramètres déjà liés sont conservés.
 *
 * @param statement Requête préparée.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_statement_reset(
    DatabaseStatement *statement
);

/**
 * @brief Efface tous les paramètres liés à une requête préparée.
 *
 * Après cet appel, les paramètres SQL redeviennent non liés et seront
 * interprétés comme NULL si la requête est exécutée sans nouveau bind.
 *
 * @param statement Requête préparée.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_statement_clear_bindings(
    DatabaseStatement *statement
);

/**
 * @brief Indique si une colonne contient la valeur SQL NULL.
 *
 * Les indices de colonnes SQLite commencent à 0.
 *
 * @param statement Requête positionnée sur une ligne.
 * @param column_index Indice de la colonne.
 * @param is_null Destination du résultat.
 *
 * @return true si la colonne a pu être inspectée, sinon false.
 */
bool database_statement_column_is_null(
    DatabaseStatement *statement,
    int column_index,
    bool *is_null
);

/**
 * @brief Lit un entier signé sur 64 bits depuis une colonne.
 *
 * Les indices de colonnes SQLite commencent à 0.
 * La colonne doit être de type SQLITE_INTEGER.
 *
 * @param statement Requête positionnée sur une ligne.
 * @param column_index Indice de la colonne.
 * @param value Destination de la valeur.
 *
 * @return true si l'entier a pu être lu, sinon false.
 */
bool database_statement_column_int64(
    DatabaseStatement *statement,
    int column_index,
    int64_t *value
);

/**
 * @brief Lit un nombre à virgule flottante depuis une colonne.
 *
 * Les indices de colonnes SQLite commencent à 0.
 *
 * La colonne doit être de type SQLITE_FLOAT ou SQLITE_INTEGER.
 * Une colonne entière est convertie en double par SQLite.
 *
 * @param statement Requête positionnée sur une ligne.
 * @param column_index Indice de la colonne.
 * @param value Destination de la valeur.
 *
 * @return true si le nombre a pu être lu, sinon false.
 */
bool database_statement_column_double(
    DatabaseStatement *statement,
    int column_index,
    double *value
);

/**
 * @brief Copie le contenu texte d'une colonne.
 *
 * Les indices de colonnes SQLite commencent à 0.
 *
 * Si la colonne contient SQL NULL, la fonction réussit et place NULL
 * dans value.
 *
 * La chaîne retournée doit être libérée avec g_free().
 *
 * @param statement Requête positionnée sur une ligne.
 * @param column_index Indice de la colonne.
 * @param value Destination de la chaîne allouée.
 *
 * @return true si la colonne a pu être lue, sinon false.
 */
bool database_statement_column_text(
    DatabaseStatement *statement,
    int column_index,
    char **value
);

/**
 * @brief Lie une chaîne de caractères à un paramètre SQL.
 *
 * Les indices SQLite commencent à 1.
 *
 * @param statement Requête préparée.
 * @param index     Indice du paramètre.
 * @param value     Chaîne à lier.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_statement_bind_text(
    DatabaseStatement *statement,
    int index,
    const char *value
);

/**
 * @brief Lie un entier signé sur 64 bits à un paramètre SQL.
 *
 * @param statement Requête préparée.
 * @param index     Indice du paramètre.
 * @param value     Valeur à lier.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_statement_bind_int64(
    DatabaseStatement *statement,
    int index,
    int64_t value
);

/**
 * @brief Lie un nombre à virgule flottante à un paramètre SQL.
 *
 * @param statement Requête préparée.
 * @param index     Indice du paramètre.
 * @param value     Valeur à lier.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_statement_bind_double(
    DatabaseStatement *statement,
    int index,
    double value
);

/**
 * @brief Lie la valeur SQL NULL à un paramètre.
 *
 * @param statement Requête préparée.
 * @param index     Indice du paramètre.
 *
 * @return true en cas de succès, sinon false.
 */
bool database_statement_bind_null(
    DatabaseStatement *statement,
    int index
);

/**
 * @brief Finalise une requête préparée et libère ses ressources.
 *
 * Cette fonction accepte NULL.
 *
 * @param statement Requête à finaliser.
 */
void database_statement_finalize(
    DatabaseStatement *statement
);

#endif
