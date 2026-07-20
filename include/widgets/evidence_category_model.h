/******************************************************************************
 * @file evidence_category_model.h
 * @brief Modèle GTK regroupant les preuves par catégorie.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_CATEGORY_MODEL_H
#define LABFY_INVESTIGATION_EVIDENCE_CATEGORY_MODEL_H

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque contenant des EvidenceCategoryItem.
 */
typedef struct EvidenceCategoryModel EvidenceCategoryModel;

/**
 * @brief Codes d'erreur produits par EvidenceCategoryModel.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ARGUMENT,

    /**
     * Le modèle contient un élément invalide.
     */
    EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ITEM,

    /**
     * Une catégorie n'a pas pu être créée.
     */
    EVIDENCE_CATEGORY_MODEL_ERROR_CATEGORY,

    /**
     * Une allocation a échoué.
     */
    EVIDENCE_CATEGORY_MODEL_ERROR_MEMORY
} EvidenceCategoryModelError;

/**
 * @brief Domaine d'erreur du modèle de catégories.
 */
#define EVIDENCE_CATEGORY_MODEL_ERROR \
    evidence_category_model_error_quark()

/**
 * @brief Retourne le domaine d'erreur du module.
 *
 * @return Quark associé à EvidenceCategoryModel.
 */
GQuark evidence_category_model_error_quark(void);

/**
 * @brief Crée un modèle de catégories vide.
 *
 * @return Nouveau modèle, ou NULL.
 */
EvidenceCategoryModel *evidence_category_model_new(void);

/**
 * @brief Libère le modèle.
 *
 * Cette fonction accepte NULL.
 *
 * @param evidence_category_model Modèle à libérer.
 */
void evidence_category_model_free(
    EvidenceCategoryModel *evidence_category_model
);

/**
 * @brief Retourne le modèle GTK contenant les catégories.
 *
 * La référence retournée est empruntée.
 *
 * @return GListModel emprunté, ou NULL.
 */
GListModel *evidence_category_model_get_list_model(
    EvidenceCategoryModel *evidence_category_model
);

/**
 * @brief Retourne le nombre de catégories.
 *
 * @return Nombre de catégories, ou 0.
 */
guint evidence_category_model_get_count(
    const EvidenceCategoryModel *evidence_category_model
);

/**
 * @brief Regroupe les preuves par type métier.
 *
 * evidence_items doit contenir des EvidenceListItem.
 *
 * type_labels est une table facultative associant :
 *
 * - clé : code métier du type ;
 * - valeur : libellé utilisateur.
 *
 * La table est uniquement empruntée pendant l'appel.
 * Lorsqu'un libellé est absent, le code métier est utilisé.
 *
 * Le nouveau contenu est entièrement préparé avant que l'ancien modèle
 * soit remplacé.
 *
 * @param evidence_category_model Modèle à actualiser.
 * @param evidence_items Modèle contenant des EvidenceListItem.
 * @param type_labels Correspondance code vers libellé, ou NULL.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE en cas de succès.
 */
gboolean evidence_category_model_replace(
    EvidenceCategoryModel *evidence_category_model,
    GListModel *evidence_items,
    GHashTable *type_labels,
    GError **error
);

G_END_DECLS

#endif
