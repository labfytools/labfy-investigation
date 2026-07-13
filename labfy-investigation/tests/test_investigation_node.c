/******************************************************************************
 * @file test_investigation_node.c
 * @brief Tests du module InvestigationNode.
 ******************************************************************************/

#include "core/investigation_node.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_directory_node(void)
{
    InvestigationNode *node = NULL;

    node = investigation_node_new(
        "01_Preuves_Originales",
        INVESTIGATION_NODE_DIRECTORY
    );

    assert(node != NULL);
    assert(strcmp(
        investigation_node_get_name(node),
        "01_Preuves_Originales"
    ) == 0);
    assert(
        investigation_node_get_type(node) ==
        INVESTIGATION_NODE_DIRECTORY
    );
    assert(investigation_node_get_parent(node) == NULL);
    assert(investigation_node_get_children_count(node) == 0);

    investigation_node_free(node);
}

static void test_file_node(void)
{
    InvestigationNode *node = NULL;

    node = investigation_node_new(
        "Chronologie.md",
        INVESTIGATION_NODE_FILE
    );

    assert(node != NULL);
    assert(strcmp(
        investigation_node_get_name(node),
        "Chronologie.md"
    ) == 0);
    assert(
        investigation_node_get_type(node) ==
        INVESTIGATION_NODE_FILE
    );

    investigation_node_free(node);
}

static void test_add_children(void)
{
    InvestigationNode *root = NULL;
    InvestigationNode *database_directory = NULL;
    InvestigationNode *database_file = NULL;
    const InvestigationNode *returned_child = NULL;

    root = investigation_node_new(
        "Template",
        INVESTIGATION_NODE_DIRECTORY
    );

    database_directory = investigation_node_new(
        "00_BaseDeDonnees",
        INVESTIGATION_NODE_DIRECTORY
    );

    database_file = investigation_node_new(
        "Enquete.sqlite",
        INVESTIGATION_NODE_FILE
    );

    assert(root != NULL);
    assert(database_directory != NULL);
    assert(database_file != NULL);

    assert(
        investigation_node_add_child(
            root,
            database_directory
        )
    );

    assert(
        investigation_node_add_child(
            database_directory,
            database_file
        )
    );

    assert(investigation_node_get_children_count(root) == 1);
    assert(
        investigation_node_get_children_count(
            database_directory
        ) == 1
    );

    returned_child = investigation_node_get_child(root, 0);

    assert(returned_child == database_directory);
    assert(
        investigation_node_get_parent(database_directory) ==
        root
    );
    assert(
        investigation_node_get_parent(database_file) ==
        database_directory
    );

    /*
     * root possède database_directory,
     * qui possède database_file.
     * La destruction est donc récursive.
     */
    investigation_node_free(root);
}

static void test_invalid_additions(void)
{
    InvestigationNode *directory = NULL;
    InvestigationNode *file = NULL;
    InvestigationNode *child = NULL;
    InvestigationNode *second_parent = NULL;

    directory = investigation_node_new(
        "Directory",
        INVESTIGATION_NODE_DIRECTORY
    );

    file = investigation_node_new(
        "File.txt",
        INVESTIGATION_NODE_FILE
    );

    child = investigation_node_new(
        "Child",
        INVESTIGATION_NODE_DIRECTORY
    );

    second_parent = investigation_node_new(
        "SecondParent",
        INVESTIGATION_NODE_DIRECTORY
    );

    assert(directory != NULL);
    assert(file != NULL);
    assert(child != NULL);
    assert(second_parent != NULL);

    assert(!investigation_node_add_child(NULL, child));
    assert(!investigation_node_add_child(directory, NULL));
    assert(!investigation_node_add_child(file, child));
    assert(!investigation_node_add_child(directory, directory));

    assert(investigation_node_add_child(directory, child));

    assert(!investigation_node_add_child(second_parent, child));

    /*
     * directory possède désormais child.
     */
    investigation_node_free(directory);
    investigation_node_free(file);
    investigation_node_free(second_parent);
}

static void test_invalid_index(void)
{
    InvestigationNode *node = NULL;

    node = investigation_node_new(
        "Template",
        INVESTIGATION_NODE_DIRECTORY
    );

    assert(node != NULL);

    assert(investigation_node_get_child(node, 0) == NULL);
    assert(investigation_node_get_child(node, 42) == NULL);
    assert(investigation_node_get_child(NULL, 0) == NULL);

    investigation_node_free(node);
}

static void test_invalid_names(void)
{
    assert(
        investigation_node_new(
            NULL,
            INVESTIGATION_NODE_FILE
        ) == NULL
    );

    assert(
        investigation_node_new(
            "",
            INVESTIGATION_NODE_FILE
        ) == NULL
    );
}

static void test_null_behaviour(void)
{
    assert(investigation_node_get_name(NULL) == NULL);
    assert(investigation_node_get_children_count(NULL) == 0);
    assert(investigation_node_get_parent(NULL) == NULL);

    investigation_node_free(NULL);
}

int main(void)
{
    test_directory_node();
    test_file_node();
    test_add_children();
    test_invalid_additions();
    test_invalid_index();
    test_invalid_names();
    test_null_behaviour();

    printf(
        "InvestigationNode : tous les tests de hiérarchie sont valides.\n"
    );

    return 0;
}
