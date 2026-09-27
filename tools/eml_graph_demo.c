/******************************************************************************
 * @file eml_graph_demo.c
 * @brief CLI bornée de génération du scénario EML synthétique.
 ******************************************************************************/

#include "core/eml_graph_specimen.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    EmlGraphSpecimenPaths paths = {0};
    EmlGraphSpecimenVariant variant = EML_GRAPH_SPECIMEN_DEFAULT;
    GError *error = NULL;
    const char *directory = NULL;
    if ((argc != 3 && argc != 5) || strcmp(argv[1], "--output-dir") != 0)
    {
        fprintf(stderr, "Usage: %s --output-dir REPERTOIRE "
            "[--variant default|alternate]\n", argv[0]);
        return 2;
    }
    directory = argv[2];
    if (argc == 5)
    {
        if (strcmp(argv[3], "--variant") != 0 ||
            (strcmp(argv[4], "default") != 0 &&
             strcmp(argv[4], "alternate") != 0))
        {
            fprintf(stderr, "Variante SPECIMEN invalide.\n");
            return 2;
        }
        variant = strcmp(argv[4], "alternate") == 0
            ? EML_GRAPH_SPECIMEN_ALTERNATE : EML_GRAPH_SPECIMEN_DEFAULT;
    }
    if (!eml_graph_specimen_generate(directory, variant, &paths, &error))
    {
        fprintf(stderr, "Génération EML impossible : %s\n",
            error != NULL ? error->message : "erreur inconnue");
        g_clear_error(&error);
        return 1;
    }
    printf("Snapshot EML C : %s\n", paths.snapshot_path);
    printf("Manifeste EML C : %s\n", paths.manifest_path);
    eml_graph_specimen_paths_clear(&paths);
    return 0;
}
