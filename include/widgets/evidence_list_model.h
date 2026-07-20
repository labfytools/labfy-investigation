/******************************************************************************
 * @file evidence_list_model.h
 * @brief Modèle GTK en mémoire contenant la liste des preuves affichées.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_LIST_MODEL_H
#define LABFY_INVESTIGATION_EVIDENCE_LIST_MODEL_H

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque contenant des EvidenceListItem.
 */
typedef struct EvidenceListModel EvidenceListModel;

/**
 * @brief Codes d'erreur produits par EvidenceListModel.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    EVIDENCE_LIST_MODEL_ERROR_INVALID_ARGUMENT,

    /**
     * Une allocation a échoué.
     */
    EVIDENCE_LIST_MODEL_ERROR_MEMORY,

    /**
     * Un EvidenceRecord ne peut pas produire un élément d'affichage.
     */
    EVIDENCE_LIST_MODEL_ERROR_INVALID_RECORD
} EvidenceListModelError;

/**
 * @brief Domaine d'erreur du modèle de liste.
 */
#define EVIDENCE_LIST_MODEL_ERROR \
    evidence_list_model_error_quark()

/**
 * @brief Retourne le domaine d'erreur du modèle.
 *
 * @return Quark GLib associé à EvidenceListModel.
 */
GQuark evidence_list_model_error_quark(void);

/**
 * @brief Crée un modèle de liste vide.
 *
 * @return Nouveau modèle, ou NULL.
 */
EvidenceListModel *evidence_list_model_new(void);

/**
 * @brief Libère le modèle et tous ses éléments.
 *
 * Cette fonction accepte NULL.
 *
 * @param evidence_list_model Modèle à libérer.
 */
void evidence_list_model_free(
    EvidenceListModel *evidence_list_model
);

/**
 * @brief Retourne le GListModel utilisé par les widgets GTK.
 *
 * La référence retournée est empruntée.
 *
 * @param evidence_list_model Modèle valide.
 *
 * @return GListModel emprunté, ou NULL.
 */
GListModel *evidence_list_model_get_list_model(
    EvidenceListModel *evidence_list_model
);

/**
 * @brief Retourne le nombre d'éléments affichés.
 *
 * @param evidence_list_model Modèle de liste.
 *
 * @return Nombre d'éléments, ou 0.
 */
guint evidence_list_model_get_count(
    const EvidenceListModel *evidence_list_model
);

/**
 * @brief Remplace complètement le contenu du modèle.
 *
 * evidence_records doit contenir des pointeurs EvidenceRecord valides.
 *
 * Tous les nouveaux EvidenceListItem sont construits avant la modification
 * du modèle existant. En cas d'erreur, l'ancien contenu reste inchangé.
 *
 * Le tableau et les EvidenceRecord restent la propriété de l'appelant.
 *
 * @param evidence_list_model Modèle à mettre à jour.
 * @param evidence_records Tableau de EvidenceRecord.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE en cas de succès.
 */
gboolean evidence_list_model_replace(
    EvidenceListModel *evidence_list_model,
    const GPtrArray *evidence_records,
    GError **error
);

G_END_DECLS

#endif
