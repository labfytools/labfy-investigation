/******************************************************************************
 * @file eml_analysis_dialog.h
 * @brief Présentation en lecture seule d'une analyse EML.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EML_ANALYSIS_DIALOG_H
#define LABFY_INVESTIGATION_EML_ANALYSIS_DIALOG_H
#include "core/eml_processing.h"
#include "core/eml_integration.h"
#include <gtk/gtk.h>
G_BEGIN_DECLS
/** @brief Callback recevant les propositions sélectionnées, ou NULL. */
typedef void (*EmlAnalysisDialogCallback)(GPtrArray *proposals,
    gpointer user_data);
/**
 * @brief Affiche les métadonnées et indicateurs extraits d'une copie EML.
 * @param parent Fenêtre parente.
 * @param result Résultat emprunté pendant la construction de la fenêtre.
 */
void eml_analysis_dialog_present(GtkWindow *parent,
    const EmlProcessingResult *result, EmlAnalysisDialogCallback callback,
    gpointer user_data);
G_END_DECLS
#endif
