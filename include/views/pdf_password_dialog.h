/******************************************************************************
 * @file pdf_password_dialog.h
 * @brief Paramétrage d'une récupération de mot de passe PDF.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_PDF_PASSWORD_DIALOG_H
#define LABFY_INVESTIGATION_PDF_PASSWORD_DIALOG_H

#include "core/pdf_password_recovery.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

/** @brief Callback recevant la méthode et son paramètre validé. */
typedef void (*PdfPasswordDialogCallback)(PdfPasswordRecoveryMethod method,
    const char *parameter, gpointer user_data);

/**
 * @brief Présente le formulaire de récupération ciblée.
 * @param parent Fenêtre parente facultative.
 * @param callback Callback appelé après validation.
 * @param user_data Données empruntées du callback.
 */
void pdf_password_dialog_present(GtkWindow *parent,
    PdfPasswordDialogCallback callback, gpointer user_data);

G_END_DECLS

#endif
