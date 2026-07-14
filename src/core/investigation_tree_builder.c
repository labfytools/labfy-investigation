/******************************************************************************
 * @file investigation_tree_builder.c
 * @brief Construction d'une arborescence d'enquête depuis le système de fichiers.
 ******************************************************************************/

#include "core/investigation_tree_builder.h"

#include "core/investigation_node.h"
#include "core/investigation_tree_model.h"

#include <stdbool.h>

#include <gio/gio.h>
#include <glib.h>

/**
 * @brief Attributs demandés à GIO pendant le parcours d'un dossier.
 */
#define INVESTIGATION_TREE_FILE_ATTRIBUTES \
    G_FILE_ATTRIBUTE_STANDARD_NAME ","      \
    G_FILE_ATTRIBUTE_STANDARD_TYPE ","      \
    G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK

/**
 * @brief Construit récursivement les enfants d'un nœud dossier.
 *
 * @param directory   Dossier GIO actuellement parcouru.
 * @param parent_node Nœud représentant ce dossier dans notre modèle.
 * @param error       Adresse recevant une éventuelle erreur GIO.
 *
 * @return true si le parcours s'est terminé correctement, sinon false.
 */
static bool investigation_tree_builder_build_children(
    GFile *directory,
    InvestigationNode *parent_node,
    GError **error
)
{
    GFileEnumerator *enumerator = NULL;
    GFileInfo *file_info = NULL;
    bool success = true;

    if (directory == NULL || parent_node == NULL)
    {
        return false;
    }

    enumerator = g_file_enumerate_children(
        directory,
        INVESTIGATION_TREE_FILE_ATTRIBUTES,
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
        NULL,
        error
    );

    if (enumerator == NULL)
    {
        return false;
    }

    while (success)
    {
        const char *child_name = NULL;
        GFileType child_file_type;
        GFile *child_file = NULL;
        InvestigationNode *child_node = NULL;
        char *child_path = NULL;
        InvestigationNodeType child_node_type;

        file_info = g_file_enumerator_next_file(
            enumerator,
            NULL,
            error
        );

        if (file_info == NULL)
        {
            /*
             * NULL signifie soit que le dossier est entièrement parcouru,
             * soit qu'une erreur a eu lieu.
             */
            if (error != NULL && *error != NULL)
            {
                success = false;
            }

            break;
        }

        /*
         * Les liens symboliques sont volontairement ignorés afin d'éviter
         * les boucles et les sorties involontaires du dossier d'enquête.
         */
        if (g_file_info_get_is_symlink(file_info))
        {
            g_object_unref(file_info);
            file_info = NULL;
            continue;
        }

        child_name = g_file_info_get_name(file_info);

        if (child_name == NULL || child_name[0] == '\0')
        {
            success = false;
            g_object_unref(file_info);
            file_info = NULL;
            break;
        }

        child_file_type = g_file_info_get_file_type(file_info);

        if (child_file_type == G_FILE_TYPE_DIRECTORY)
        {
            child_node_type = INVESTIGATION_NODE_DIRECTORY;
        }
        else
        {
            child_node_type = INVESTIGATION_NODE_FILE;
        }

        child_file = g_file_get_child(
            directory,
            child_name
        );

        if (child_file == NULL)
        {
            success = false;
            g_object_unref(file_info);
            file_info = NULL;
            break;
        }

        child_path = g_file_get_path(child_file);

        if (child_path == NULL)
        {
            success = false;
            g_object_unref(child_file);
            g_object_unref(file_info);
            file_info = NULL;
            break;
        }

        child_node = investigation_node_new(
            child_name,
            child_path,
            child_node_type
        );

        g_free(child_path);
        child_path = NULL;
        
        /*
         * En cas de succès, parent_node devient propriétaire de child_node.
         */
        if (!investigation_node_add_child(parent_node, child_node))
        {
            investigation_node_free(child_node);
            success = false;
            g_object_unref(child_file);
            g_object_unref(file_info);
            file_info = NULL;
            break;
        }

        /*
         * Seuls les dossiers sont parcourus récursivement.
         */
        if (child_node_type == INVESTIGATION_NODE_DIRECTORY)
        {
            success = investigation_tree_builder_build_children(
                child_file,
                child_node,
                error
            );
        }

        g_object_unref(child_file);
        g_object_unref(file_info);

        child_file = NULL;
        file_info = NULL;
    }

    /*
     * Si la boucle a été interrompue avant la libération de file_info,
     * cette vérification garantit un nettoyage correct.
     */
    if (file_info != NULL)
    {
        g_object_unref(file_info);
    }

    g_object_unref(enumerator);

    return success;
}

InvestigationTreeModel *investigation_tree_builder_build(
    const char *root_path
)
{
    GFile *root_file = NULL;
    GFileType root_file_type;
    char *root_name = NULL;
    InvestigationNode *root_node = NULL;
    InvestigationTreeModel *tree_model = NULL;
    GError *error = NULL;

    if (root_path == NULL || root_path[0] == '\0')
    {
        return NULL;
    }

    root_file = g_file_new_for_path(root_path);

    if (root_file == NULL)
    {
        return NULL;
    }

    root_file_type = g_file_query_file_type(
        root_file,
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
        NULL
    );

    if (root_file_type != G_FILE_TYPE_DIRECTORY)
    {
        g_object_unref(root_file);
        return NULL;
    }

    root_name = g_file_get_basename(root_file);

    if (root_name == NULL || root_name[0] == '\0')
    {
        g_free(root_name);
        g_object_unref(root_file);
        return NULL;
    }

    root_node = investigation_node_new(
        root_name,
        root_path,
        INVESTIGATION_NODE_DIRECTORY
    );

    g_free(root_name);

    if (root_node == NULL)
    {
        g_object_unref(root_file);
        return NULL;
    }

    if (!investigation_tree_builder_build_children(
            root_file,
            root_node,
            &error
        ))
    {
        if (error != NULL)
        {
            g_warning(
                "Impossible de construire l'arborescence : %s",
                error->message
            );

            g_clear_error(&error);
        }

        investigation_node_free(root_node);
        g_object_unref(root_file);

        return NULL;
    }

    g_object_unref(root_file);

    tree_model = investigation_tree_model_new(root_node);

    if (tree_model == NULL)
    {
        /*
         * Le transfert de propriété n'a pas eu lieu puisque la création
         * du modèle a échoué.
         */
        investigation_node_free(root_node);
        return NULL;
    }

    return tree_model;
}
