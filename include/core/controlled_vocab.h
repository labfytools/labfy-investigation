/******************************************************************************
 * @file controlled_vocab.h
 * @brief Gestion du vocabulaire contrôlé pour les domaines fermés.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_CONTROLLED_VOCAB_H
#define LABFY_INVESTIGATION_CONTROLLED_VOCAB_H

#include <glib.h>
#include <stddef.h>

G_BEGIN_DECLS

/** @brief Élément d'un vocabulaire contrôlé. */
typedef struct ControlledVocabItem
{
    const char *code;  /**< Code technique stable en anglais */
    const char *label; /**< Libellé affichable en français */
} ControlledVocabItem;

/** @brief Catégories de vocabulaire contrôlé. */
typedef enum ControlledVocabCategory
{
    CONTROLLED_VOCAB_VERIFICATION_STATUS = 0,
    CONTROLLED_VOCAB_PROVENANCE_KIND,
    CONTROLLED_VOCAB_EMAIL_ROLE,
    CONTROLLED_VOCAB_SMTP_ROLE,
    CONTROLLED_VOCAB_VALUE_TYPE,
    CONTROLLED_VOCAB_COUNT
} ControlledVocabCategory;

/**
 * @brief Retourne la liste ordonnée d'un vocabulaire contrôlé.
 * @param category Catégorie demandée.
 * @param out_count Pointeur recevant le nombre d'éléments.
 * @return Tableau d'éléments statiques, ou NULL si catégorie invalide.
 */
const ControlledVocabItem *controlled_vocab_get_items(ControlledVocabCategory category,
                                                       size_t *out_count);

/**
 * @brief Vérifie si un code appartient à une catégorie de vocabulaire.
 */
gboolean controlled_vocab_is_valid_code(ControlledVocabCategory category,
                                        const char *code);

/**
 * @brief Obtient le libellé utilisateur (français) à partir d'un code technique.
 */
const char *controlled_vocab_get_label(ControlledVocabCategory category,
                                       const char *code);

/**
 * @brief Obtient le code technique à partir d'un libellé utilisateur.
 */
const char *controlled_vocab_get_code_from_label(ControlledVocabCategory category,
                                                 const char *label);

/* Helpers spécifiques pour les catégories principales */
gboolean controlled_vocab_is_valid_verification_status(const char *code);
gboolean controlled_vocab_is_valid_provenance_kind(const char *code);
gboolean controlled_vocab_is_valid_email_role(const char *code);
gboolean controlled_vocab_is_valid_smtp_role(const char *code);
gboolean controlled_vocab_is_valid_value_type(const char *code);

G_END_DECLS

#endif /* LABFY_INVESTIGATION_CONTROLLED_VOCAB_H */
