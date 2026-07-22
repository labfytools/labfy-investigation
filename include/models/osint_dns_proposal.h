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

/**
 * @brief Retourne le type d'entité compatible avec la proposition.
 *
 * Les types actuellement pris en charge sont `ip_address` pour A et AAAA,
 * ainsi que `domain_name` pour CNAME, NS et PTR.
 *
 * @param proposal Proposition consultée.
 *
 * @return Identifiant statique du type d'entité, ou NULL.
 */
const char *osint_dns_proposal_get_entity_type(
    const OsintDnsProposal *proposal
);

/**
 * @brief Normalise la valeur avant recherche ou insertion.
 *
 * Les adresses IP sont converties dans leur représentation canonique. Les
 * noms de domaine sont passés en minuscules et leur point final est retiré.
 *
 * @param proposal Proposition consultée.
 *
 * @return Nouvelle chaîne à libérer avec g_free(), ou NULL si la proposition
 *         n'est pas compatible ou si sa valeur est invalide.
 */
char *osint_dns_proposal_dup_normalized_value(
    const OsintDnsProposal *proposal
);

/**
 * @brief Retourne le type de relation correspondant au type DNS.
 *
 * A, AAAA et PTR utilisent `resolves_to`, CNAME utilise `aliases_to` et NS
 * utilise `uses_name_server`.
 *
 * @param proposal Proposition consultée.
 *
 * @return Identifiant statique de relation, ou NULL.
 */
const char *osint_dns_proposal_get_relation_type(
    const OsintDnsProposal *proposal
);

G_END_DECLS

#endif
