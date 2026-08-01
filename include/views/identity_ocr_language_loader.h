#ifndef IDENTITY_OCR_LANGUAGE_LOADER_H
#define IDENTITY_OCR_LANGUAGE_LOADER_H

#include <gtk/gtk.h>

void identity_ocr_language_loader_start(GtkWindow *window, const char *tesseract_path, GtkDropDown *language_dropdown);

#endif
