/******************************************************************************
 * @file relation_type_normalizer.h
 * @brief Normalisation métier des libellés de types de relations.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_RELATION_TYPE_NORMALIZER_H
#define LABFY_INVESTIGATION_RELATION_TYPE_NORMALIZER_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Produit la clé d'identité Unicode d'un libellé de type.
 *
 * La fonction applique NFC, replie toute suite d'espaces Unicode en un espace,
 * supprime les espaces périphériques puis effectue un casefold Unicode.
 *
 * @return Nouvelle chaîne à libérer avec g_free(), ou NULL si le libellé est
 * invalide ou vide.
 */
char *relation_type_normalize_key(const char *label);

G_END_DECLS

#endif
