/******************************************************************************
 * @file relation_service.h
 * @brief Orchestration transactionnelle de la création des relations.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_RELATION_SERVICE_H
#define LABFY_INVESTIGATION_RELATION_SERVICE_H

#include "database/database.h"
#include "models/relation_record.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Service opaque de gestion transactionnelle des relations.
 *
 * Le service emprunte la connexion Database reçue à sa création.
 */
typedef struct RelationService RelationService;

/**
 * @brief Catégories d'erreurs produites par RelationService.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    RELATION_SERVICE_ERROR_INVALID_ARGUMENT,

    /**
     * Une allocation mémoire a échoué.
     */
    RELATION_SERVICE_ERROR_MEMORY,

    /**
     * La transaction SQLite n'a pas pu être démarrée.
     */
    RELATION_SERVICE_ERROR_TRANSACTION_BEGIN,

    /**
     * La relation n'a pas pu être insérée.
     */
    RELATION_SERVICE_ERROR_RELATION_INSERT,

    /**
     * Une preuve n'a pas pu être associée à la relation.
     */
    RELATION_SERVICE_ERROR_EVIDENCE_LINK,

    /**
     * La transaction SQLite n'a pas pu être validée.
     */
    RELATION_SERVICE_ERROR_TRANSACTION_COMMIT,

    /**
     * La transaction SQLite n'a pas pu être annulée.
     */
    RELATION_SERVICE_ERROR_TRANSACTION_ROLLBACK
} RelationServiceError;

/**
 * @brief Domaine d'erreur du service.
 */
#define RELATION_SERVICE_ERROR \
    relation_service_error_quark()

/**
 * @brief Retourne le domaine d'erreur du service.
 */
GQuark relation_service_error_quark(void);

/**
 * @brief Crée un service utilisant une connexion Database existante.
 *
 * Le service emprunte la connexion et possède les DAO qu'il crée.
 *
 * @param database Connexion SQLite ouverte et initialisée.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau service, ou NULL en cas d'échec.
 */
RelationService *relation_service_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le service.
 *
 * Les DAO internes sont libérés, mais la connexion empruntée n'est
 * jamais fermée.
 *
 * Cette fonction accepte NULL.
 *
 * @param relation_service Service à libérer.
 */
void relation_service_free(
    RelationService *relation_service
);

/**
 * @brief Crée atomiquement une relation et ses associations aux preuves.
 *
 * relation_record, evidence_identifiers et les chaînes du tableau sont
 * empruntés pendant l'appel.
 *
 * evidence_identifiers peut être NULL ou vide.
 *
 * Un retour TRUE garantit que la relation et toutes ses associations ont
 * été validées par COMMIT.
 *
 * Un retour FALSE signifie que l'opération complète a échoué.
 *
 * @param relation_service Service valide.
 * @param relation_record Relation à insérer.
 * @param evidence_identifiers Tableau facultatif d'UUID de preuves.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'opération complète est validée.
 */
gboolean relation_service_create(
    RelationService *relation_service,
    const RelationRecord *relation_record,
    const GPtrArray *evidence_identifiers,
    GError **error
);

G_END_DECLS

#endif
