/******************************************************************************
 * @file core_graph_demo.c
 * @brief Commande locale de génération du snapshot C synthétique J3.
 ******************************************************************************/

#include "core/core_graph_specimen.h"

#include <glib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    CoreGraphSpecimenPaths paths = {0};
    GError *error = NULL;
    if (argc != 3 || g_strcmp0(argv[1], "--output-dir") != 0)
    {
        fprintf(stderr, "Usage: %s --output-dir REPERTOIRE_PRIVE_NEUF\n", argv[0]);
        return 2;
    }
    if (!core_graph_specimen_generate(argv[2], &paths, &error))
    {
        fprintf(stderr, "Échec de génération J3 : %s\n",
            error != NULL ? error->message : "erreur inconnue");
        g_clear_error(&error);
        return 1;
    }
    /* Sortie machine étroite : le pont local lit uniquement ces chemins. */
    printf("SNAPSHOT=%s\nMANIFEST=%s\nDATABASE=%s\n",
        paths.snapshot_path, paths.manifest_path, paths.database_path);
    core_graph_specimen_paths_clear(&paths);
    return 0;
}
