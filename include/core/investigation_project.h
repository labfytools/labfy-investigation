/******************************************************************************
 * @file investigation_project.h
 * @brief Interface publique de gestion d'un projet d'enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_PROJECT_H
#define LABFY_INVESTIGATION_INVESTIGATION_PROJECT_H

#include <stdbool.h>

/**
 * @brief Crée une nouvelle enquête dans un dossier parent.
 *
 * La fonction crée :
 *
 * - le dossier racine de l'enquête ;
 * - l'arborescence standard ;
 * - le fichier vide 00_BaseDeDonnees/Enquete.sqlite.
 *
 * Le dossier parent doit déjà exister.
 *
 * Le nom de l'enquête ne doit pas être vide et ne doit pas contenir
 * de séparateur de chemin.
 *
 * En cas d'échec, les éléments créés pendant l'appel sont supprimés.
 * Aucun dossier existant avant l'appel ne doit être supprimé.
 *
 * La chaîne retournée appartient au code appelant et doit être libérée
 * avec g_free().
 *
 * @param parent_directory  Chemin du dossier parent.
 * @param investigation_name Nom de la nouvelle enquête.
 *
 * @return Le chemin complet de l'enquête créée, ou NULL en cas d'échec.
 */
char *investigation_project_create(
    const char *parent_directory,
    const char *investigation_name
);

/******************************************************************************
 * @brief Vérifie qu'un dossier est une enquête valide.
 *
 * La fonction contrôle que tous les éléments obligatoires de la structure
 * d'une enquête existent et sont du type attendu.
 *
 * Aucune modification n'est effectuée sur le système de fichiers.
 *
 * @param investigation_path Chemin de l'enquête à vérifier.
 *
 * @return true si l'enquête est valide, sinon false.
 ******************************************************************************/
bool investigation_project_validate(
    const char *investigation_path
);

#endif
