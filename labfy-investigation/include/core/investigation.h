/******************************************************************************
 * @file investigation.h
 * @brief Interface publique du module Investigation.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_H
#define LABFY_INVESTIGATION_INVESTIGATION_H

/**
 * @brief Représentation opaque d'une enquête.
 *
 * La structure réelle est définie dans investigation.c.
 * Les autres modules peuvent manipuler un pointeur vers Investigation,
 * mais ne peuvent pas accéder directement à ses champs internes.
 */
typedef struct Investigation Investigation;

/**
 * @brief Crée une nouvelle enquête à partir d'un dossier racine.
 *
 * @param root_path Chemin du dossier racine de l'enquête.
 *
 * @return Une nouvelle enquête, ou NULL si le chemin est invalide
 *         ou si l'allocation mémoire échoue.
 */
Investigation *investigation_new(const char *root_path);

/**
 * @brief Libère toutes les ressources associées à une enquête.
 *
 * Cette fonction accepte NULL.
 *
 * @param investigation Enquête à libérer.
 */
void investigation_free(Investigation *investigation);

/**
 * @brief Retourne le chemin racine de l'enquête.
 *
 * @param investigation Enquête à consulter.
 *
 * @return Le chemin racine en lecture seule, ou NULL si l'enquête est invalide.
 */
const char *investigation_get_root_path(
    const Investigation *investigation
);

/**
 * @brief Retourne le chemin de la base de données de l'enquête.
 *
 * @param investigation Enquête à consulter.
 *
 * @return Le chemin de la base en lecture seule, ou NULL si l'enquête est invalide.
 */
const char *investigation_get_database_path(
    const Investigation *investigation
);

#endif
