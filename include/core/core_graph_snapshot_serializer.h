/******************************************************************************
 * @file core_graph_snapshot_serializer.h
 * @brief Sérialisation JSON v2 et publication atomique d'un snapshot cœur.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_CORE_GRAPH_SNAPSHOT_SERIALIZER_H
#define LABFY_INVESTIGATION_CORE_GRAPH_SNAPSHOT_SERIALIZER_H

#include "models/core_graph_snapshot.h"

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    CORE_GRAPH_SERIALIZER_ERROR_INVALID_ARGUMENT,
    CORE_GRAPH_SERIALIZER_ERROR_INVALID_UTF8,
    CORE_GRAPH_SERIALIZER_ERROR_LIMIT,
    CORE_GRAPH_SERIALIZER_ERROR_GENERATION,
    CORE_GRAPH_SERIALIZER_ERROR_PUBLICATION
} CoreGraphSerializerError;

#define CORE_GRAPH_SERIALIZER_ERROR core_graph_serializer_error_quark()
GQuark core_graph_serializer_error_quark(void);

/**
 * Produit le contrat labfy.web_graph.snapshot.v2 avec JSON-GLib. Le booléen
 * synthetic_fixture décrit le mode explicite du contexte appelant. FALSE
 * désigne un espace local expérimental ouvert par le service C ; le
 * sérialiseur ne tente jamais de déduire ce mode du contenu ou du chemin.
 * La chaîne retournée appartient à l'appelant.
 */
char *core_graph_snapshot_serialize(
    const CoreGraphSnapshot *snapshot,
    gboolean synthetic_fixture,
    gsize *out_length,
    GError **error
);

/**
 * Écrit d'abord dans un fichier privé voisin puis publie par renommage. En cas
 * d'échec, le fichier final n'est jamais créé à partir d'un JSON partiel.
 */
gboolean core_graph_snapshot_write_atomic(
    const CoreGraphSnapshot *snapshot,
    gboolean synthetic_fixture,
    const char *output_path,
    GError **error
);

G_END_DECLS

#endif
