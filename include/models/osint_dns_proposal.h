/******************************************************************************
 * @file osint_dns_proposal.h
 * @brief Propositions structurées extraites d'une sortie DNS brute.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_DNS_PROPOSAL_H
#define LABFY_INVESTIGATION_OSINT_DNS_PROPOSAL_H

#include <glib.h>

G_BEGIN_DECLS

/** @brief Proposition DNS opaque produite avant toute intégration. */
typedef struct OsintDnsProposal OsintDnsProposal;

/**
 * @brief Extrait les enregistrements reconnus d'une sortie `dig +answer`.
 *
 * Le tableau retourné possède ses propositions et doit être libéré avec
 * g_ptr_array_unref(). Les lignes vides, les commentaires et les lignes
 * invalides sont ignorés sans modifier la sortie brute d'origine.
 *
 * @param target_value Cible DNS ayant produit la sortie.
 * @param raw_output Sortie standard UTF-8 de dig.
 *
 * @return Nouveau tableau, éventuellement vide, ou NULL si les arguments
 *         sont invalides.
 */
GPtrArray *osint_dns_proposal_parse(
    const char *target_value,
    const char *raw_output
);

/**
 * @brief Retourne le propriétaire de l'enregistrement.
 *
 * @param proposal Proposition consultée.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *osint_dns_proposal_get_owner(
    const OsintDnsProposal *proposal
);

/**
 * @brief Retourne le type d'enregistrement DNS.
 *
 * @param proposal Proposition consultée.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *osint_dns_proposal_get_record_type(
    const OsintDnsProposal *proposal
);

/**
 * @brief Retourne la valeur brute de l'enregistrement.
 *
 * @param proposal Proposition consultée.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *osint_dns_proposal_get_value(
    const OsintDnsProposal *proposal
);

/**
 * @brief Retourne la cible initialement interrogée.
 *
 * @param proposal Proposition consultée.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *osint_dns_proposal_get_target(
    const OsintDnsProposal *proposal
);

G_END_DECLS

#endif
