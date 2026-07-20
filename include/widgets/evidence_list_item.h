/******************************************************************************
 * @file evidence_list_item.h
 * @brief Objet d'affichage GTK représentant une preuve importée.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_LIST_ITEM_H
#define LABFY_INVESTIGATION_EVIDENCE_LIST_ITEM_H

#include "models/evidence_record.h"

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * @brief Type GObject de EvidenceListItem.
 */
#define EVIDENCE_TYPE_LIST_ITEM \
    (evidence_list_item_get_type())

G_DECLARE_FINAL_TYPE(
    EvidenceListItem,
    evidence_list_item,
    EVIDENCE,
    LIST_ITEM,
    GObject
)

/**
 * @brief Crée un élément d'affichage depuis une preuve métier.
 *
 * Les valeurs nécessaires à l'interface sont copiées.
 * L'objet ne conserve aucun pointeur emprunté vers EvidenceRecord.
 *
 * L'appelant possède la référence retournée et doit la libérer
 * avec g_object_unref() ou g_clear_object().
 *
 * @param evidence_record Preuve métier valide.
 *
 * @return Nouvel élément d'affichage, ou NULL.
 */
EvidenceListItem *evidence_list_item_new(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne l'identifiant UUID de la preuve.
 *
 * Cet identifiant est utilisé pour signaler une sélection à Application.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_list_item_get_identifier(
    const EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne le nom original de la preuve.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_list_item_get_original_name(
    const EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne l'identifiant métier du type de preuve.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_list_item_get_type_identifier(
    const EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne la taille de la preuve.
 *
 * @return Taille en octets, ou 0.
 */
guint64 evidence_list_item_get_size_bytes(
    const EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne la date UTC de collecte.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_list_item_get_collected_at(
    const EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne la source déclarée.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_list_item_get_source(
    const EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne l'état d'intégrité.
 *
 * @return État d'intégrité enregistré.
 */
EvidenceIntegrityStatus evidence_list_item_get_integrity_status(
    const EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne un libellé utilisateur pour l'état d'intégrité.
 *
 * La chaîne retournée est statique et ne doit pas être libérée.
 *
 * @return Libellé d'état.
 */
const char *evidence_list_item_get_integrity_status_label(
    const EvidenceListItem *evidence_list_item
);

G_END_DECLS

#endif
