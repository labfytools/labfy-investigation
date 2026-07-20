/******************************************************************************
 * @file evidence_import_dialog.h
 * @brief Dialogue GTK de saisie des métadonnées d'une preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_IMPORT_DIALOG_H
#define LABFY_INVESTIGATION_EVIDENCE_IMPORT_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Résultat opaque produit par le dialogue d'import.
 */
typedef struct EvidenceImportDialogResult EvidenceImportDialogResult;

/**
 * @brief Codes d'erreur produits par EvidenceImportDialog.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_ARGUMENT,

    /**
     * Aucun type de preuve n'est disponible.
     */
    EVIDENCE_IMPORT_DIALOG_ERROR_EMPTY_TYPES,

    /**
     * Une allocation ou la création du dialogue a échoué.
     */
    EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,

    /**
     * Les métadonnées saisies sont invalides.
     */
    EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA
} EvidenceImportDialogError;

/**
 * @brief Domaine d'erreur du dialogue d'import.
 */
#define EVIDENCE_IMPORT_DIALOG_ERROR \
    evidence_import_dialog_error_quark()

/**
 * @brief Callback appelé après la fermeture du dialogue.
 *
 * En cas de validation, result est non NULL et appartient au callback.
 * Il doit être libéré avec evidence_import_dialog_result_free().
 *
 * En cas d'annulation, result vaut NULL.
 *
 * @param result Résultat possédé par le callback, ou NULL.
 * @param user_data Données privées fournies lors de l'ouverture.
 */
typedef void (*EvidenceImportDialogCallback)(
    EvidenceImportDialogResult *result,
    gpointer user_data
);

/**
 * @brief Retourne le domaine d'erreur du dialogue.
 *
 * @return Quark GLib associé à EvidenceImportDialog.
 */
GQuark evidence_import_dialog_error_quark(void);

/**
 * @brief Présente le dialogue de saisie des métadonnées.
 *
 * Le chemin du fichier est affiché en lecture seule.
 *
 * evidence_types doit contenir des pointeurs EvidenceType valides.
 * Le dialogue copie les codes et les libellés dont il a besoin :
 * le tableau peut donc être libéré après le retour de cette fonction.
 *
 * Le type dont le code vaut "document" est sélectionné par défaut
 * lorsqu'il est disponible. Sinon, le premier type est sélectionné.
 *
 * @param parent Fenêtre GTK parente.
 * @param file_path Chemin du fichier sélectionné.
 * @param evidence_types Tableau de EvidenceType.
 * @param callback Fonction appelée après validation ou annulation.
 * @param user_data Données privées transmises au callback.
 * @param error Emplacement facultatif recevant une erreur immédiate.
 *
 * @return TRUE si le dialogue a été créé et présenté.
 */
gboolean evidence_import_dialog_present(
    GtkWindow *parent,
    const char *file_path,
    const GPtrArray *evidence_types,
    EvidenceImportDialogCallback callback,
    gpointer user_data,
    GError **error
);

/**
 * @brief Valide les valeurs obligatoires d'un import.
 *
 * Cette fonction ne dépend pas de l'affichage du dialogue et peut être
 * testée sans ouvrir de fenêtre GTK.
 *
 * Le chemin doit désigner un fichier existant.
 * Le code du type doit être non vide.
 * La date doit être une date ISO 8601 valide comprenant un fuseau horaire.
 *
 * @param file_path Chemin du fichier à importer.
 * @param type_identifier Code métier du type de preuve.
 * @param collected_at Date de collecte au format ISO 8601.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE lorsque les valeurs sont valides.
 */
gboolean evidence_import_dialog_validate_values(
    const char *file_path,
    const char *type_identifier,
    const char *collected_at,
    GError **error
);

/**
 * @brief Libère le résultat produit par le dialogue.
 *
 * Cette fonction accepte NULL.
 *
 * @param result Résultat à libérer.
 */
void evidence_import_dialog_result_free(
    EvidenceImportDialogResult *result
);

/**
 * @brief Retourne le code du type sélectionné.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_import_dialog_result_get_type_identifier(
    const EvidenceImportDialogResult *result
);

/**
 * @brief Retourne la date ISO 8601 de collecte.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_import_dialog_result_get_collected_at(
    const EvidenceImportDialogResult *result
);

/**
 * @brief Retourne la source renseignée.
 *
 * @return Chaîne empruntée, ou NULL lorsque la source est vide.
 */
const char *evidence_import_dialog_result_get_source(
    const EvidenceImportDialogResult *result
);

/**
 * @brief Retourne la description renseignée.
 *
 * @return Chaîne empruntée, ou NULL lorsque la description est vide.
 */
const char *evidence_import_dialog_result_get_description(
    const EvidenceImportDialogResult *result
);

G_END_DECLS

#endif
