/******************************************************************************
 * @file research_store.h
 * @brief Persistance V4 des autorisations de recherche assistée.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RESEARCH_STORE_H
#define LABFY_INVESTIGATION_RESEARCH_STORE_H

#include "core/local_job_store.h"
#include "core/research_contracts.h"

G_BEGIN_DECLS

/**
 * Admet atomiquement un plan et ses seeds/actions exactes.
 * CONTRACT: même clé et même contenu retourne le même objet durable; une même
 * clé avec un contenu différent est un conflit.
 * INVARIANT: aucune action invalide ne laisse un plan partiellement écrit.
 */
gboolean research_store_admit_plan(LocalJobStore *store,
                                   const ResearchPlan *plan,
                                   gboolean *out_reused, GError **error);

/**
 * Admet un grant lié au plan et matérialise les sujets/provider/endpoints
 * autorisés au moment de la décision. Toutes les décisions humaines sont
 * persistées dans la même transaction. REFUSE est terminal et rend l'action
 * durablement inadmissible; la vague 2 exige une vague 1 autorisée, exécutée
 * et documentée par son résultat et son reçu.
 */
gboolean research_store_admit_grant(LocalJobStore *store,
                                    const ScopeGrant *grant,
                                    const ResearchActionDecision *decisions,
                                    gsize decision_count,
                                    const char *decided_at,
                                    gboolean *out_reused, GError **error);

/** Révoque durablement un grant sans supprimer son historique. */
gboolean research_store_revoke_grant(LocalJobStore *store,
                                     const char *grant_id,
                                     const char *revoked_at,
                                     GError **error);

/** Admet l'identité durable d'une campagne; ne démarre aucun travail. */
gboolean research_store_admit_campaign(LocalJobStore *store,
                                       const ResearchCampaign *campaign,
                                       gboolean *out_reused, GError **error);

/**
 * Enregistre atomiquement le résultat brut et le reçu de policy correspondant.
 * INVARIANT: le reçu doit viser une action explicitement sélectionnée du grant
 * de la campagne; aucun résultat orphelin n'est publié.
 */
gboolean research_store_record_result(LocalJobStore *store,
                                      const ResearchResult *result,
                                      const ResearchReceipt *receipt,
                                      GError **error);

/**
 * Évalue un usage exact du grant sans créer de job ni réserver de budget.
 * WHY: l'unique worker existant reste le seul ordonnanceur; cette fonction
 * rend la décision de policy observable avant toute admission ultérieure.
 * INVARIANT: exclusion, révocation et expiration sont des refus explicites;
 * redirection, CNAME ou nouvelle URL ne correspondent jamais par construction.
 */
gboolean research_store_policy_decide(LocalJobStore *store,
                                      const char *grant_id,
                                      const ResearchPolicyRequest *request,
                                      ResearchPolicyDecision *out_decision,
                                      GError **error);

G_END_DECLS
#endif
