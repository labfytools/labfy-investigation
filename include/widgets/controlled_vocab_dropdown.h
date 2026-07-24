/******************************************************************************
 * @file controlled_vocab_dropdown.h
 * @brief Composant GTK4 (GtkDropDown) basé sur le vocabulaire contrôlé.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_CONTROLLED_VOCAB_DROPDOWN_H
#define LABFY_INVESTIGATION_CONTROLLED_VOCAB_DROPDOWN_H

#include "core/controlled_vocab.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Crée un widget GtkDropDown initialisé avec une catégorie de vocabulaire contrôlé.
 * @param category Catégorie de vocabulaire.
 * @param default_code Code sélectionné par défaut (facultatif).
 * @return Nouveau GtkWidget (GtkDropDown).
 */
GtkWidget *controlled_vocab_dropdown_new(ControlledVocabCategory category,
                                         const char *default_code);

/**
 * @brief Obtient le code technique sélectionné dans le dropdown.
 * @param dropdown Widget GtkDropDown créé avec controlled_vocab_dropdown_new.
 * @return Code technique emprunté, ou NULL si rien n'est sélectionné.
 */
const char *controlled_vocab_dropdown_get_selected_code(GtkWidget *dropdown);

/**
 * @brief Sélectionne un code technique dans le dropdown.
 * @param dropdown Widget GtkDropDown.
 * @param code Code à sélectionner.
 * @return TRUE si le code a été trouvé et sélectionné, FALSE sinon.
 */
gboolean controlled_vocab_dropdown_set_selected_code(GtkWidget *dropdown,
                                                      const char *code);

G_END_DECLS

#endif /* LABFY_INVESTIGATION_CONTROLLED_VOCAB_DROPDOWN_H */
