/******************************************************************************
 * @file investigation_graph_loader.h
 * @brief Chargement du graphe métier d'une enquête depuis SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LOADER_H
#define LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LOADER_H

#include "database/database.h"
#include "models/investigation_graph_model.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Chargeur opaque du graphe métier d'une enquête.
 *
 * Le chargeur emprunte la connexion Database reçue lors de sa création.
 * Il possède les DAO qu'il utilise.
 */
typedef struct InvestigationGraphLoader InvestigationGraphLoader;

/**
 * @brief Catégories d'erreurs produites par InvestigationGraphLoader.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    INVESTIGATION_GRAPH_LOADER_ERROR_INVALID_ARGUMENT,

    /**
     * Une allocation ou la création du graphe a échoué.
     */
    INVESTIGATION_GRAPH_LOADER_ERROR_MEMORY,

    /**
     * Les entités n'ont pas pu être chargées depuis SQLite.
     */
    INVESTIGATION_GRAPH_LOADER_ERROR_ENTITY_LOAD,

    /**
     * Une entité n'a pas pu être transférée dans le graphe.
     */
    INVESTIGATION_GRAPH_LOADER_ERROR_ENTITY_TRANSFER,

    /**
     * Les relations n'ont pas pu être chargées depuis SQLite.
     */
    INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_LOAD,

    /**
     * Une relation n'a pas pu être transférée dans le graphe.
     */
    INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_TRANSFER
} InvestigationGraphLoaderError;

/**
 * @brief Domaine d'erreur du chargeur.
 */
#define INVESTIGATION_GRAPH_LOADER_ERROR \
    investigation_graph_loader_error_quark()

/**
 * @brief Retourne le domaine d'erreur du chargeur.
 */
GQuark investigation_graph_loader_error_quark(void);

/**
 * @brief Crée un chargeur utilisant une connexion Database existante.
 *
 * La connexion est empruntée et doit rester valide pendant toute la durée de
 * vie du chargeur.
 *
 * @param database Connexion SQLite ouverte et initialisée.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau chargeur, ou NULL en cas d'échec.
 */
InvestigationGraphLoader *investigation_graph_loader_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le chargeur.
 *
 * Les DAO internes sont libérés, mais la connexion Database empruntée n'est
 * jamais fermée.
 *
 * Cette fonction accepte NULL.
 *
 * @param graph_loader Chargeur à libérer.
 */
void investigation_graph_loader_free(
    InvestigationGraphLoader *graph_loader
);

/**
 * @brief Construit un nouveau graphe depuis les données SQLite.
 *
 * Toutes les entités sont chargées et transférées avant les relations.
 *
 * Le graphe retourné appartient à l'appelant. Au moindre échec, le graphe
 * partiellement construit et tous les modèles temporaires sont détruits.
 *
 * @param graph_loader Chargeur valide.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau graphe complet, ou NULL en cas d'échec.
 */
InvestigationGraphModel *investigation_graph_loader_load(
    InvestigationGraphLoader *graph_loader,
    GError **error
);

G_END_DECLS

#endif
