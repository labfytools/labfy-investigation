/******************************************************************************
 * @file evidence_category_item.h
 * @brief Catégorie GTK regroupant plusieurs preuves affichées.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_CATEGORY_ITEM_H
#define LABFY_INVESTIGATION_EVIDENCE_CATEGORY_ITEM_H

#include "widgets/evidence_list_item.h"

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * @brief Type GObject de EvidenceCategoryItem.
 */
#define EVIDENCE_TYPE_CATEGORY_ITEM \
    (evidence_category_item_get_type())

G_DECLARE_FINAL_TYPE(
    EvidenceCategoryItem,
    evidence_category_item,
    EVIDENCE,
    CATEGORY_ITEM,
    GObject
)

/**
 * @brief Crée une catégorie de preuves.
 *
 * Le code et le libellé sont copiés et nettoyés.
 *
 * @param type_identifier Code métier stable de la catégorie.
 * @param label Libellé destiné à l'utilisateur.
 *
 * @return Nouvelle catégorie, ou NULL.
 */
EvidenceCategoryItem *evidence_category_item_new(
    const char *type_identifier,
    const char *label
);

/**
 * @brief Ajoute une preuve à la catégorie.
 *
 * La catégorie conserve sa propre référence sur evidence_list_item.
 *
 * @param evidence_category_item Catégorie cible.
 * @param evidence_list_item Preuve d'affichage à ajouter.
 *
 * @return TRUE en cas de succès.
 */
gboolean evidence_category_item_append(
    EvidenceCategoryItem *evidence_category_item,
    EvidenceListItem *evidence_list_item
);

/**
 * @brief Retourne le code métier de la catégorie.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_category_item_get_type_identifier(
    const EvidenceCategoryItem *evidence_category_item
);

/**
 * @brief Retourne le libellé utilisateur.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_category_item_get_label(
    const EvidenceCategoryItem *evidence_category_item
);

/**
 * @brief Retourne le nombre de preuves de la catégorie.
 *
 * @return Nombre de preuves, ou 0.
 */
guint evidence_category_item_get_count(
    const EvidenceCategoryItem *evidence_category_item
);

/**
 * @brief Retourne le modèle contenant les preuves.
 *
 * La référence retournée est empruntée.
 *
 * @return GListModel emprunté, ou NULL.
 */
GListModel *evidence_category_item_get_children(
    EvidenceCategoryItem *evidence_category_item
);

G_END_DECLS

#endif
