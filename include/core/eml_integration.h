/******************************************************************************
 * @file eml_integration.h
 * @brief Intégration transactionnelle de propositions issues d'un EML.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EML_INTEGRATION_H
#define LABFY_INVESTIGATION_EML_INTEGRATION_H
#include "database/database.h"
#include <glib.h>
G_BEGIN_DECLS
/** @brief Proposition d'entité explicitement sélectionnable. */
typedef struct { char *type_identifier; char *value; } EmlEntityProposal;
/** @brief Crée une proposition possédée. */
EmlEntityProposal *eml_entity_proposal_new(const char *type_identifier,
    const char *value);
/** @brief Libère une proposition. */
void eml_entity_proposal_free(EmlEntityProposal *proposal);
/**
 * @brief Intègre atomiquement des entités et les lie à la preuve EML.
 * @param database Connexion ouverte empruntée.
 * @param evidence_identifier UUID de la preuve source.
 * @param proposals Tableau de EmlEntityProposal sélectionnées.
 * @param out_created Nombre d'entités créées.
 * @param out_reused Nombre d'entités existantes réutilisées.
 * @param error Destination facultative d'une erreur.
 * @return TRUE après validation de la transaction.
 */
gboolean eml_integration_apply(Database *database,
    const char *evidence_identifier, const GPtrArray *proposals,
    guint *out_created, guint *out_reused, GError **error);
G_END_DECLS
#endif
