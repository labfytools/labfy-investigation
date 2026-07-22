/******************************************************************************
 * @file rib_ocr_review_dialog.h
 * @brief Révision humaine des résultats OCR d'un RIB.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RIB_OCR_REVIEW_DIALOG_H
#define LABFY_INVESTIGATION_RIB_OCR_REVIEW_DIALOG_H
#include <gtk/gtk.h>
G_BEGIN_DECLS
typedef void (*RibOcrReviewCallback)(const char *iban, gpointer user_data);
/** @brief Présente le texte OCR et un IBAN révisable. */
void rib_ocr_review_dialog_present(GtkWindow *parent, const char *ocr_text,
    const char *suggested_iban, RibOcrReviewCallback callback,
    gpointer user_data);
G_END_DECLS
#endif
