/******************************************************************************
 * @file investigation_tree_builder.h
 * @brief Interface publique du constructeur d'arborescence d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_TREE_BUILDER_H
#define LABFY_INVESTIGATION_INVESTIGATION_TREE_BUILDER_H

#include "core/investigation_tree_model.h"

/**
 * @brief Construit le modèle d'arborescence d'une enquête depuis le disque.
 *
 * Le chemin fourni doit désigner un dossier existant.
 *
 * La fonction parcourt récursivement son contenu et crée :
 *
 * - un nœud racine pour le dossier sélectionné ;
 * - un nœud de type DIRECTORY pour chaque sous-dossier ;
 * - un nœud de type FILE pour chaque fichier.
 *
 * Les liens symboliques ne sont pas suivis.
 *
 * Le code appelant devient propriétaire du modèle retourné et doit le
 * libérer avec investigation_tree_model_free().
 *
 * @param root_path Chemin du dossier racine à parcourir.
 *
 * @return Un nouveau modèle d'arborescence, ou NULL si le chemin est invalide
 *         ou si une erreur empêche la construction du modèle.
 */
InvestigationTreeModel *investigation_tree_builder_build(
    const char *root_path
);

#endif
