/******************************************************************************
 * @file extraction_drop_service.h
 * @brief Validation et intégration d'une extraction déposée sur le graphe.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EXTRACTION_DROP_SERVICE_H
#define LABFY_INVESTIGATION_EXTRACTION_DROP_SERVICE_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

typedef struct ExtractionDropInfo ExtractionDropInfo;

typedef enum
{
    EXTRACTION_DROP_SERVICE_ERROR_INVALID_ARGUMENT,
    EXTRACTION_DROP_SERVICE_ERROR_OUTSIDE_INVESTIGATION,
    EXTRACTION_DROP_SERVICE_ERROR_INVALID_FILE,
    EXTRACTION_DROP_SERVICE_ERROR_READ,
    EXTRACTION_DROP_SERVICE_ERROR_DATABASE
} ExtractionDropServiceError;

#define EXTRACTION_DROP_SERVICE_ERROR extraction_drop_service_error_quark()

GQuark extraction_drop_service_error_quark(void);

/**
 * @brief Valide et lit une extraction texte appartenant à l'enquête.
 *
 * Le fichier doit être un `.txt` régulier situé sous
 * `02_Preuves_Traitees/Extractions`. Aucun fichier n'est déplacé ou modifié.
 */
ExtractionDropInfo *extraction_drop_service_prepare(
    const char *investigation_root,
    const char *file_path,
    GError **error
);

void extraction_drop_info_free(ExtractionDropInfo *info);
const char *extraction_drop_info_get_file_path(const ExtractionDropInfo *info);
const char *extraction_drop_info_get_relative_path(
    const ExtractionDropInfo *info);
const char *extraction_drop_info_get_file_name(const ExtractionDropInfo *info);
const char *extraction_drop_info_get_source(const ExtractionDropInfo *info);
const char *extraction_drop_info_get_content(const ExtractionDropInfo *info);

/**
 * @brief Enregistre l'extraction comme preuve traitée et la lie à une entité.
 *
 * Une preuve ou une association déjà présente est réutilisée.
 */
gboolean extraction_drop_service_attach(
    Database *database,
    const ExtractionDropInfo *info,
    const char *entity_identifier,
    GError **error
);

/**
 * @brief Crée une entité générique puis lui associe l'extraction.
 *
 * @return UUID nouvellement alloué dans out_entity_identifier, si demandé.
 */
gboolean extraction_drop_service_create_entity(
    Database *database,
    const ExtractionDropInfo *info,
    char **out_entity_identifier,
    GError **error
);

G_END_DECLS

#endif
