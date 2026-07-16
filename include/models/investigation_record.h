/******************************************************************************
 * @file investigation_record.h
 * @brief Modèle représentant les informations persistées d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_RECORD_H
#define LABFY_INVESTIGATION_INVESTIGATION_RECORD_H

/**
 * @brief Modèle opaque représentant une ligne de la table investigation.
 */
typedef struct InvestigationRecord InvestigationRecord;

/**
 * @brief Crée un modèle représentant une enquête persistée.
 *
 * Toutes les chaînes sont copiées par le modèle.
 *
 * @param id Identifiant UUID de l'enquête.
 * @param name Nom de l'enquête.
 * @param root_path Chemin racine de l'enquête.
 * @param created_at Date de création.
 * @param updated_at Date de dernière modification.
 *
 * @return Un nouveau modèle, ou NULL si les données sont invalides
 *         ou si une allocation échoue.
 */
InvestigationRecord *investigation_record_new(
    const char *id,
    const char *name,
    const char *root_path,
    const char *created_at,
    const char *updated_at
);

/**
 * @brief Libère un modèle InvestigationRecord.
 *
 * Cette fonction accepte NULL.
 *
 * @param record Modèle à libérer.
 */
void investigation_record_free(
    InvestigationRecord *record
);

/**
 * @brief Retourne l'identifiant de l'enquête.
 *
 * @return Identifiant appartenant au modèle, ou NULL.
 */
const char *investigation_record_get_id(
    const InvestigationRecord *record
);

/**
 * @brief Retourne le nom de l'enquête.
 *
 * @return Nom appartenant au modèle, ou NULL.
 */
const char *investigation_record_get_name(
    const InvestigationRecord *record
);

/**
 * @brief Retourne le chemin racine de l'enquête.
 *
 * @return Chemin appartenant au modèle, ou NULL.
 */
const char *investigation_record_get_root_path(
    const InvestigationRecord *record
);

/**
 * @brief Retourne la date de création.
 *
 * @return Date appartenant au modèle, ou NULL.
 */
const char *investigation_record_get_created_at(
    const InvestigationRecord *record
);

/**
 * @brief Retourne la date de dernière modification.
 *
 * @return Date appartenant au modèle, ou NULL.
 */
const char *investigation_record_get_updated_at(
    const InvestigationRecord *record
);

#endif
