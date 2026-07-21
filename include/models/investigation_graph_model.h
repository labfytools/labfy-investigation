/******************************************************************************
 * @file investigation_graph_model.h
 * @brief Graphe métier en mémoire d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_GRAPH_MODEL_H
#define LABFY_INVESTIGATION_INVESTIGATION_GRAPH_MODEL_H

#include "models/entity_record.h"
#include "models/relation_record.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Graphe métier opaque d'une enquête.
 *
 * Le graphe possède les EntityRecord et RelationRecord ajoutés avec succès.
 * Il ne contient aucune donnée d'affichage ou de disposition graphique.
 */
typedef struct InvestigationGraphModel InvestigationGraphModel;

/**
 * @brief Catégories d'erreurs produites par InvestigationGraphModel.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,

    /**
     * Une allocation mémoire a échoué.
     */
    INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,

    /**
     * Une entité possède un UUID déjà présent.
     */
    INVESTIGATION_GRAPH_MODEL_ERROR_DUPLICATE_ENTITY,

    /**
     * Une relation possède un UUID déjà présent.
     */
    INVESTIGATION_GRAPH_MODEL_ERROR_DUPLICATE_RELATION,

    /**
     * L'entité source d'une relation est absente.
     */
    INVESTIGATION_GRAPH_MODEL_ERROR_SOURCE_NOT_FOUND,

    /**
     * L'entité cible d'une relation est absente.
     */
    INVESTIGATION_GRAPH_MODEL_ERROR_TARGET_NOT_FOUND,

    /**
     * L'entité demandée est absente.
     */
    INVESTIGATION_GRAPH_MODEL_ERROR_ENTITY_NOT_FOUND
} InvestigationGraphModelError;

/**
 * @brief Domaine d'erreur du modèle de graphe.
 */
#define INVESTIGATION_GRAPH_MODEL_ERROR \
    investigation_graph_model_error_quark()

/**
 * @brief Retourne le domaine d'erreur du modèle.
 */
GQuark investigation_graph_model_error_quark(void);

/**
 * @brief Crée un graphe métier vide.
 *
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau graphe, ou NULL en cas d'échec.
 */
InvestigationGraphModel *investigation_graph_model_new(
    GError **error
);

/**
 * @brief Libère le graphe et tous les modèles qu'il possède.
 *
 * Cette fonction accepte NULL.
 *
 * @param graph_model Graphe à libérer.
 */
void investigation_graph_model_free(
    InvestigationGraphModel *graph_model
);

/**
 * @brief Ajoute une entité au graphe.
 *
 * En cas de succès, le graphe prend possession de entity_record.
 * En cas d'échec, l'appelant conserve la propriété du modèle.
 *
 * @param graph_model Graphe valide.
 * @param entity_record Entité à transférer.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'entité est ajoutée.
 */
gboolean investigation_graph_model_add_entity(
    InvestigationGraphModel *graph_model,
    EntityRecord *entity_record,
    GError **error
);

/**
 * @brief Ajoute une relation orientée au graphe.
 *
 * Les entités source et cible doivent déjà exister.
 *
 * En cas de succès, le graphe prend possession de relation_record.
 * En cas d'échec, l'appelant conserve la propriété du modèle.
 *
 * @param graph_model Graphe valide.
 * @param relation_record Relation à transférer.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si la relation est ajoutée.
 */
gboolean investigation_graph_model_add_relation(
    InvestigationGraphModel *graph_model,
    RelationRecord *relation_record,
    GError **error
);

/**
 * @brief Recherche une entité par UUID.
 *
 * Le pointeur retourné est emprunté au graphe.
 *
 * @param graph_model Graphe à consulter.
 * @param entity_identifier UUID recherché.
 *
 * @return Entité empruntée, ou NULL.
 */
const EntityRecord *investigation_graph_model_find_entity(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier
);

/**
 * @brief Recherche une relation par UUID.
 *
 * Le pointeur retourné est emprunté au graphe.
 *
 * @param graph_model Graphe à consulter.
 * @param relation_identifier UUID recherché.
 *
 * @return Relation empruntée, ou NULL.
 */
const RelationRecord *investigation_graph_model_find_relation(
    const InvestigationGraphModel *graph_model,
    const char *relation_identifier
);

/**
 * @brief Retourne le nombre d'entités du graphe.
 *
 * @return Nombre d'entités, ou 0 pour un graphe NULL.
 */
guint64 investigation_graph_model_get_entity_count(
    const InvestigationGraphModel *graph_model
);

/**
 * @brief Retourne le nombre de relations du graphe.
 *
 * @return Nombre de relations, ou 0 pour un graphe NULL.
 */
guint64 investigation_graph_model_get_relation_count(
    const InvestigationGraphModel *graph_model
);

/**
 * @brief Liste toutes les entités par UUID croissant.
 *
 * Le tableau appartient à l'appelant. Ses éléments sont empruntés au graphe
 * et ne doivent pas être libérés.
 *
 * @param graph_model Graphe valide.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau tableau de const EntityRecord *, ou NULL en cas d'échec.
 */
GPtrArray *investigation_graph_model_list_entities(
    const InvestigationGraphModel *graph_model,
    GError **error
);

/**
 * @brief Liste toutes les relations par UUID croissant.
 *
 * Le tableau appartient à l'appelant. Ses éléments sont empruntés au graphe
 * et ne doivent pas être libérés.
 *
 * @param graph_model Graphe valide.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau tableau de const RelationRecord *, ou NULL en cas d'échec.
 */
GPtrArray *investigation_graph_model_list_relations(
    const InvestigationGraphModel *graph_model,
    GError **error
);

/**
 * @brief Liste les relations sortantes d'une entité.
 *
 * Le tableau appartient à l'appelant. Les relations restent la propriété
 * du graphe.
 *
 * @param graph_model Graphe valide.
 * @param entity_identifier UUID de l'entité source.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau tableau trié, ou NULL en cas d'échec.
 */
GPtrArray *investigation_graph_model_list_outgoing_relations(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier,
    GError **error
);

/**
 * @brief Liste les relations entrantes d'une entité.
 *
 * Le tableau appartient à l'appelant. Les relations restent la propriété
 * du graphe.
 *
 * @param graph_model Graphe valide.
 * @param entity_identifier UUID de l'entité cible.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau tableau trié, ou NULL en cas d'échec.
 */
GPtrArray *investigation_graph_model_list_incoming_relations(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier,
    GError **error
);

/**
 * @brief Liste toutes les relations incidentes d'une entité.
 *
 * Une relation ne peut apparaître qu'une seule fois dans le tableau.
 *
 * Le tableau appartient à l'appelant. Les relations restent la propriété
 * du graphe.
 *
 * @param graph_model Graphe valide.
 * @param entity_identifier UUID de l'entité.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau tableau trié, ou NULL en cas d'échec.
 */
GPtrArray *investigation_graph_model_list_incident_relations(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier,
    GError **error
);

G_END_DECLS

#endif
