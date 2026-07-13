/******************************************************************************
 * @file test_investigation_tree_builder.c
 * @brief Tests d'intégration du module InvestigationTreeBuilder.
 ******************************************************************************/

#include "core/investigation_node.h"
#include "core/investigation_tree_builder.h"
#include "core/investigation_tree_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Recherche un enfant par son nom.
 *
 * @param parent Nœud dans lequel effectuer la recherche.
 * @param name   Nom recherché.
 *
 * @return L'enfant correspondant, ou NULL s'il n'existe pas.
 */
static const InvestigationNode *test_find_child(
    const InvestigationNode *parent,
    const char *name
)
{
    size_t child_count = 0;

    if (parent == NULL || name == NULL)
    {
        return NULL;
    }

    child_count = investigation_node_get_children_count(parent);

    for (size_t index = 0; index < child_count; ++index)
    {
        const InvestigationNode *child = NULL;
        const char *child_name = NULL;

        child = investigation_node_get_child(parent, index);

        if (child == NULL)
        {
            continue;
        }

        child_name = investigation_node_get_name(child);

        if (child_name != NULL && strcmp(child_name, name) == 0)
        {
            return child;
        }
    }

    return NULL;
}

/**
 * @brief Crée un fichier texte utilisé par le test.
 */
static void test_create_file(const char *file_path)
{
    GError *error = NULL;

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "test\n",
            -1,
            &error
        )
    );

    assert(error == NULL);
}

/**
 * @brief Vérifie la construction récursive d'une arborescence.
 */
static void test_build_tree_from_filesystem(void)
{
    char *root_path = NULL;
    char *root_name = NULL;

    char *directory_a_path = NULL;
    char *directory_b_path = NULL;
    char *file_a_path = NULL;
    char *root_file_path = NULL;

    GError *error = NULL;

    InvestigationTreeModel *tree_model = NULL;
    const InvestigationNode *root_node = NULL;
    const InvestigationNode *directory_a_node = NULL;
    const InvestigationNode *directory_b_node = NULL;
    const InvestigationNode *file_a_node = NULL;
    const InvestigationNode *root_file_node = NULL;

    /*
     * GLib crée un dossier temporaire unique, par exemple :
     *
     * /tmp/labfy-investigation-test-ABC123
     */
    root_path = g_dir_make_tmp(
        "labfy-investigation-test-XXXXXX",
        &error
    );

    assert(root_path != NULL);
    assert(error == NULL);

    directory_a_path = g_build_filename(
        root_path,
        "DirectoryA",
        NULL
    );

    directory_b_path = g_build_filename(
        root_path,
        "DirectoryB",
        NULL
    );

    file_a_path = g_build_filename(
        directory_a_path,
        "FileA.txt",
        NULL
    );

    root_file_path = g_build_filename(
        root_path,
        "RootFile.md",
        NULL
    );

    assert(g_mkdir(directory_a_path, 0700) == 0);
    assert(g_mkdir(directory_b_path, 0700) == 0);

    test_create_file(file_a_path);
    test_create_file(root_file_path);

    /*
     * Arborescence créée :
     *
     * racine temporaire/
     * ├── DirectoryA/
     * │   └── FileA.txt
     * ├── DirectoryB/
     * └── RootFile.md
     */
    tree_model = investigation_tree_builder_build(root_path);

    assert(tree_model != NULL);

    root_node = investigation_tree_model_get_root(tree_model);

    assert(root_node != NULL);
    assert(
        investigation_node_get_type(root_node) ==
        INVESTIGATION_NODE_DIRECTORY
    );

    root_name = g_path_get_basename(root_path);

    assert(root_name != NULL);
    assert(
        strcmp(
            investigation_node_get_name(root_node),
            root_name
        ) == 0
    );

    assert(investigation_node_get_children_count(root_node) == 3);

    /*
     * L'ordre retourné par le système de fichiers n'est pas garanti.
     * On recherche donc chaque enfant par son nom.
     */
    directory_a_node = test_find_child(
        root_node,
        "DirectoryA"
    );

    directory_b_node = test_find_child(
        root_node,
        "DirectoryB"
    );

    root_file_node = test_find_child(
        root_node,
        "RootFile.md"
    );

    assert(directory_a_node != NULL);
    assert(directory_b_node != NULL);
    assert(root_file_node != NULL);

    assert(
        investigation_node_get_type(directory_a_node) ==
        INVESTIGATION_NODE_DIRECTORY
    );

    assert(
        investigation_node_get_type(directory_b_node) ==
        INVESTIGATION_NODE_DIRECTORY
    );

    assert(
        investigation_node_get_type(root_file_node) ==
        INVESTIGATION_NODE_FILE
    );

    assert(
        investigation_node_get_parent(directory_a_node) ==
        root_node
    );

    assert(
        investigation_node_get_parent(directory_b_node) ==
        root_node
    );

    assert(
        investigation_node_get_parent(root_file_node) ==
        root_node
    );

    /*
     * DirectoryB est vide.
     */
    assert(
        investigation_node_get_children_count(directory_b_node) == 0
    );

    /*
     * DirectoryA contient FileA.txt.
     */
    assert(
        investigation_node_get_children_count(directory_a_node) == 1
    );

    file_a_node = test_find_child(
        directory_a_node,
        "FileA.txt"
    );

    assert(file_a_node != NULL);

    assert(
        investigation_node_get_type(file_a_node) ==
        INVESTIGATION_NODE_FILE
    );

    assert(
        investigation_node_get_parent(file_a_node) ==
        directory_a_node
    );

    /*
     * Le modèle possède la racine et toute son arborescence.
     */
    investigation_tree_model_free(tree_model);

    /*
     * Nettoyage du système de fichiers dans l'ordre inverse
     * de la création.
     */
    assert(g_remove(file_a_path) == 0);
    assert(g_remove(root_file_path) == 0);
    assert(g_rmdir(directory_a_path) == 0);
    assert(g_rmdir(directory_b_path) == 0);
    assert(g_rmdir(root_path) == 0);

    g_free(root_file_path);
    g_free(file_a_path);
    g_free(directory_b_path);
    g_free(directory_a_path);
    g_free(root_name);
    g_free(root_path);
}

/**
 * @brief Vérifie les chemins invalides.
 */
static void test_invalid_paths(void)
{
    char *temporary_file_path = NULL;
    GError *error = NULL;

    assert(investigation_tree_builder_build(NULL) == NULL);
    assert(investigation_tree_builder_build("") == NULL);

    assert(
        investigation_tree_builder_build(
            "/chemin/qui/nexiste/pas"
        ) == NULL
    );

    temporary_file_path = g_build_filename(
        g_get_tmp_dir(),
        "labfy-investigation-builder-file-test.txt",
        NULL
    );

    test_create_file(temporary_file_path);

    /*
     * Le chemin existe, mais il désigne un fichier et non un dossier.
     */
    assert(
        investigation_tree_builder_build(
            temporary_file_path
        ) == NULL
    );

    assert(g_remove(temporary_file_path) == 0);

    g_free(temporary_file_path);
    g_clear_error(&error);
}

int main(void)
{
    test_build_tree_from_filesystem();
    test_invalid_paths();

    printf(
        "InvestigationTreeBuilder : tous les tests sont valides.\n"
    );

    return 0;
}
