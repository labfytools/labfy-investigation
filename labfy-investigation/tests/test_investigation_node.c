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

static void test_null_free(void)
{
    investigation_node_free(NULL);
}

int main(void)
{
    test_directory_node();
    test_file_node();
    test_invalid_names();
    test_null_free();

    printf("InvestigationNode : tous les tests sont valides.\n");

    return 0;
}
