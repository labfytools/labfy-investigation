/******************************************************************************
 * @file create_relation_dialog.h
 * @brief Dialogue GTK de préparation d'une relation entre deux entités.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_CREATE_RELATION_DIALOG_H
#define LABFY_INVESTIGATION_CREATE_RELATION_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Résultat opaque produit par le dialogue.
 */
typedef struct CreateRelationDialogResult CreateRelationDialogResult;
typedef struct RelationRecord RelationRecord;

/**
 * @brief Codes d'erreur produits par CreateRelationDialog.
 */
typedef enum
{
    CREATE_RELATION_DIALOG_ERROR_INVALID_ARGUMENT,
    CREATE_RELATION_DIALOG_ERROR_EMPTY_TARGETS,
    CREATE_RELATION_DIALOG_ERROR_MEMORY,
    CREATE_RELATION_DIALOG_ERROR_INVALID_DATA
} CreateRelationDialogError;

/**
 * @brief Domaine d'erreur du dialogue.
 */
#define CREATE_RELATION_DIALOG_ERROR \
    create_relation_dialog_error_quark()

/**
 * @brief Callback appelé après validation ou annulation.
 *
 * En cas de validation, result appartient au callback et doit être libéré
 * avec create_relation_dialog_result_free().
 *
 * En cas d'annulation, result vaut NULL.
 *
 * @param result Résultat possédé par le callback, ou NULL.
 * @param user_data Données privées fournies lors de l'ouverture.
 */
typedef void (*CreateRelationDialogCallback)(
    CreateRelationDialogResult *result,
    gpointer user_data
);

/**
 * @brief Retourne le domaine d'erreur du dialogue.
 */
GQuark create_relation_dialog_error_quark(void);

/**
 * @brief Présente le dialogue de création d'une relation.
 *
 * entities doit contenir des pointeurs EntityRecord valides. Le dialogue
 * copie tous les identifiants et libellés nécessaires et ne conserve aucun
 * pointeur vers les modèles fournis.
 *
 * L'entité source est retirée de la liste des cibles.
 *
 * Cette fonction ne crée aucune relation et n'accède pas à SQLite.
 *
 * @param parent Fenêtre GTK parente.
 * @param source_entity_identifier UUID de l'entité source.
 * @param entities Tableau de EntityRecord.
 * @param evidence_records Tableau de EvidenceRecord disponibles.
 * @param investigation_root_path Racine canonique de l'enquête.
 * @param callback Fonction appelée après validation ou annulation.
 * @param user_data Données privées transmises au callback.
 * @param error Emplacement facultatif recevant une erreur immédiate.
 *
 * @return TRUE si le dialogue a été créé et présenté.
 */
gboolean create_relation_dialog_present(
    GtkWindow *parent,
    const char *source_entity_identifier,
    const GPtrArray *entities,
    const GPtrArray *evidence_records,
    const char *investigation_root_path,
    CreateRelationDialogCallback callback,
    gpointer user_data,
    GError **error
);

/**
 * @brief Présente le dialogue prérempli pour modifier une relation.
 *
 * La source et la cible sont conservées ; les champs métier sont modifiables.
 * Le dialogue ne réalise aucune écriture SQLite.
 *
 * @param parent Fenêtre GTK parente.
 * @param relation_record Relation existante à modifier.
 * @param entities Tableau de EntityRecord.
 * @param evidence_records Tableau de EvidenceRecord disponibles.
 * @param selected_evidence_identifiers UUID des preuves déjà liées.
 * @param investigation_root_path Racine canonique de l'enquête.
 * @param callback Fonction appelée après validation ou annulation.
 * @param user_data Données privées transmises au callback.
 * @param error Emplacement facultatif recevant une erreur immédiate.
 *
 * @return TRUE si le dialogue a été créé et présenté.
 */
gboolean edit_relation_dialog_present(
    GtkWindow *parent,
    const RelationRecord *relation_record,
    const GPtrArray *entities,
    const GPtrArray *evidence_records,
    const GPtrArray *selected_evidence_identifiers,
    const char *investigation_root_path,
    CreateRelationDialogCallback callback,
    gpointer user_data,
    GError **error
);

/**
 * @brief Valide les valeurs obligatoires d'une relation.
 *
 * Cette fonction est indépendante de l'affichage et pourra être testée
 * sans ouvrir de fenêtre GTK.
 *
 * @param source_entity_identifier UUID de l'entité source.
 * @param target_entity_identifier UUID de l'entité cible.
 * @param relation_type Type de relation non vide.
 * @param confidence Confiance comprise entre 0 et 100.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE lorsque les valeurs sont valides.
 */
gboolean create_relation_dialog_validate_values(
    const char *source_entity_identifier,
    const char *target_entity_identifier,
    const char *relation_type,
    gint confidence,
    GError **error
);

/**
 * @brief Libère un résultat.
 *
 * Cette fonction accepte NULL.
 */
void create_relation_dialog_result_free(
    CreateRelationDialogResult *result
);

/**
 * @brief Retourne l'UUID de l'entité source.
 */
const char *create_relation_dialog_result_get_source_identifier(
    const CreateRelationDialogResult *result
);

/**
 * @brief Retourne l'UUID de l'entité cible.
 */
const char *create_relation_dialog_result_get_target_identifier(
    const CreateRelationDialogResult *result
);

/**
 * @brief Retourne le type de relation.
 */
const char *create_relation_dialog_result_get_relation_type(
    const CreateRelationDialogResult *result
);

/**
 * @brief Retourne le libellé facultatif.
 */
const char *create_relation_dialog_result_get_label(
    const CreateRelationDialogResult *result
);

/**
 * @brief Retourne la justification facultative.
 */
const char *create_relation_dialog_result_get_justification(
    const CreateRelationDialogResult *result
);

/**
 * @brief Retourne le niveau de confiance.
 */
gint create_relation_dialog_result_get_confidence(
    const CreateRelationDialogResult *result
);

/**
 * @brief Retourne les UUID de preuves sélectionnés.
 *
 * @param result Résultat validé du dialogue.
 *
 * @return Tableau emprunté d'UUID, ou NULL.
 */
const GPtrArray *create_relation_dialog_result_get_evidence_identifiers(
    const CreateRelationDialogResult *result);

G_END_DECLS

#endif
