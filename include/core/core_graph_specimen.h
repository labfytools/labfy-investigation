/******************************************************************************
 * @file core_graph_specimen.h
 * @brief Générateur de démonstration strictement synthétique J3.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_CORE_GRAPH_SPECIMEN_H
#define LABFY_INVESTIGATION_CORE_GRAPH_SPECIMEN_H

#include <glib.h>

G_BEGIN_DECLS

#define CORE_GRAPH_SPECIMEN_PERSON_A "10000000-0000-4000-8000-000000000031"
#define CORE_GRAPH_SPECIMEN_PERSON_B "10000000-0000-4000-8000-000000000032"
#define CORE_GRAPH_SPECIMEN_EMAIL "20000000-0000-4000-8000-000000000031"
#define CORE_GRAPH_SPECIMEN_DOMAIN "20000000-0000-4000-8000-000000000032"
#define CORE_GRAPH_SPECIMEN_EVIDENCE_A "30000000-0000-4000-8000-000000000031"
#define CORE_GRAPH_SPECIMEN_EVIDENCE_B "30000000-0000-4000-8000-000000000032"
#define CORE_GRAPH_SPECIMEN_RELATION "40000000-0000-4000-8000-000000000031"
#define CORE_GRAPH_SPECIMEN_EXECUTION "50000000-0000-4000-8000-000000000031"

typedef struct
{
    char *database_path;
    char *snapshot_path;
    char *manifest_path;
} CoreGraphSpecimenPaths;

/** Libère les trois chemins possédés. */
void core_graph_specimen_paths_clear(CoreGraphSpecimenPaths *paths);

/**
 * Construit une base V20 neuve via Database/DAO, ferme le writer, rouvre en
 * lecture seule puis publie snapshot et manifeste. output_directory doit être
 * un répertoire privé existant et Enquete.sqlite doit être absent.
 */
gboolean core_graph_specimen_generate(
    const char *output_directory,
    CoreGraphSpecimenPaths *out_paths,
    GError **error
);

G_END_DECLS

#endif
