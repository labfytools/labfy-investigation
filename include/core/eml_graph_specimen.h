/******************************************************************************
 * @file eml_graph_specimen.h
 * @brief Démonstration synthétique analyse EML vers graphe Web.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EML_GRAPH_SPECIMEN_H
#define LABFY_INVESTIGATION_EML_GRAPH_SPECIMEN_H

#include <glib.h>

G_BEGIN_DECLS

#define EML_GRAPH_REQUEST_PRIMARY "61000000-0000-4000-8000-000000000031"
#define EML_GRAPH_REQUEST_REANALYSIS "61000000-0000-4000-8000-000000000032"
#define EML_GRAPH_DERIVATIVE_PRIMARY "62000000-0000-4000-8000-000000000031"
#define EML_GRAPH_DERIVATIVE_REANALYSIS "62000000-0000-4000-8000-000000000032"

typedef enum
{
    EML_GRAPH_SPECIMEN_DEFAULT,
    EML_GRAPH_SPECIMEN_ALTERNATE
} EmlGraphSpecimenVariant;

typedef struct
{
    char *database_path;
    char *snapshot_path;
    char *manifest_path;
} EmlGraphSpecimenPaths;

void eml_graph_specimen_paths_clear(EmlGraphSpecimenPaths *paths);

/**
 * Crée une fixture V20 neuve, importe réellement son EML, exécute deux
 * analyses persistées et exporte le snapshot. Le répertoire doit être privé,
 * existant et ne pas déjà contenir Enquete.sqlite.
 */
gboolean eml_graph_specimen_generate(
    const char *output_directory,
    EmlGraphSpecimenVariant variant,
    EmlGraphSpecimenPaths *out_paths,
    GError **error);

G_END_DECLS

#endif
