/******************************************************************************
 * @file osint_dns_integration.h
 * @brief Intégration transactionnelle de propositions DNS validées.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_DNS_INTEGRATION_H
#define LABFY_INVESTIGATION_OSINT_DNS_INTEGRATION_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

/** @brief Codes d'erreur de l'intégration DNS. */
typedef enum
{
    OSINT_DNS_INTEGRATION_ERROR_INVALID_ARGUMENT,
    OSINT_DNS_INTEGRATION_ERROR_DATABASE,
    OSINT_DNS_INTEGRATION_ERROR_MODEL,
    OSINT_DNS_INTEGRATION_ERROR_INSERT
} OsintDnsIntegrationError;

/** @brief Domaine d'erreur de l'intégration DNS. */
#define OSINT_DNS_INTEGRATION_ERROR osint_dns_integration_error_quark()

/**
 * @brief Retourne le domaine d'erreur de l'intégration DNS.
 *
 * @return Quark GLib du domaine d'erreur.
 */
GQuark osint_dns_integration_error_quark(void);

/**
 * @brief Intègre une sélection de propositions dans une transaction unique.
 *
 * Les propositions doivent être des OsintDnsProposal empruntées. Les valeurs
 * incompatibles sont ignorées. Les couples type/valeur déjà présents sont
 * comptés comme doublons et ne sont pas réinsérés.
 *
 * @param database Connexion SQLite active.
 * @param selected_proposals Propositions explicitement sélectionnées.
 * @param out_inserted_count Nombre d'entités créées.
 * @param out_skipped_count Nombre de propositions ignorées ou déjà présentes.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE lorsque la transaction est validée, sinon FALSE.
 */
gboolean osint_dns_integration_apply(
    Database *database,
    GPtrArray *selected_proposals,
    guint *out_inserted_count,
    guint *out_skipped_count,
    GError **error
);

G_END_DECLS

#endif
