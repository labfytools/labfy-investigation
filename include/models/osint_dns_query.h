/******************************************************************************
 * @file osint_dns_query.h
 * @brief Validation des cibles utilisées pour une résolution DNS OSINT.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_DNS_QUERY_H
#define LABFY_INVESTIGATION_OSINT_DNS_QUERY_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Vérifie qu'une cible est un nom de domaine DNS exploitable.
 *
 * La fonction accepte les lettres ASCII sans distinction de casse, les
 * chiffres, les tirets internes et les points séparant les labels. Un point
 * final représentant la racine DNS est également accepté.
 *
 * @param target_value Valeur à vérifier.
 *
 * @return TRUE lorsque la cible peut être transmise à dig.
 */
gboolean osint_dns_query_is_valid_target(
    const char *target_value
);

G_END_DECLS

#endif
