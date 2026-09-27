/******************************************************************************
 * @file local_toolkit_specimen.h
 * @brief Démonstration intégrée EML natif + ExifTool réel vers graphe.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_LOCAL_TOOLKIT_SPECIMEN_H
#define LABFY_INVESTIGATION_LOCAL_TOOLKIT_SPECIMEN_H

#include <glib.h>

G_BEGIN_DECLS
gboolean local_toolkit_specimen_generate(
    const char *output_directory, GError **error);
G_END_DECLS
#endif
