/******************************************************************************
 * @file osint_dns_review_dialog.h
 * @brief Dialogue de sélection des propositions issues d'une réponse DNS.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_DNS_REVIEW_DIALOG_H
#define LABFY_INVESTIGATION_OSINT_DNS_REVIEW_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Callback appelé après confirmation de la sélection DNS.
 *
 * Le tableau et les propositions sont empruntés et uniquement valides pendant
 * l'appel. Le tableau contient seulement les propositions compatibles cochées.
 *
 * @param selected_proposals Propositions sélectionnées.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*OsintDnsReviewDialogConfirmCallback)(
    GPtrArray *selected_proposals,
    gpointer user_data
);

/**
 * @brief Affiche le dialogue de sélection avant intégration.
 *
 * Le dialogue conserve une référence sur proposals jusqu'à sa fermeture.
 * Fermer ou annuler ne déclenche pas le callback.
 *
 * @param parent_window Fenêtre parente, ou NULL.
 * @param proposals Tableau de OsintDnsProposal à réviser.
 * @param callback Callback de confirmation facultatif.
 * @param user_data Données empruntées transmises au callback.
 */
void osint_dns_review_dialog_present(
    GtkWindow *parent_window,
    GPtrArray *proposals,
    OsintDnsReviewDialogConfirmCallback callback,
    gpointer user_data
);

G_END_DECLS

#endif
