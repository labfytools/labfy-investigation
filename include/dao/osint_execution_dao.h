/******************************************************************************
 * @file osint_execution_dao.h
 * @brief Persistance de la provenance des exécutions OSINT.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_EXECUTION_DAO_H
#define LABFY_INVESTIGATION_OSINT_EXECUTION_DAO_H

#include "database/database.h"
#include "models/osint_execution_record.h"

G_BEGIN_DECLS

/** @brief DAO opaque empruntant une connexion Database. */
typedef struct OsintExecutionDao OsintExecutionDao;

/** @brief Crée un DAO sur une connexion existante. */
OsintExecutionDao *osint_execution_dao_new(Database *database, GError **error);
/** @brief Libère le DAO sans fermer la connexion. */
void osint_execution_dao_free(OsintExecutionDao *dao);
/** @brief Insère une exécution sans remplacer une ligne existante. */
gboolean osint_execution_dao_insert(
    OsintExecutionDao *dao, const OsintExecutionRecord *record, GError **error
);
/** @brief Recherche une exécution par UUID et retourne un modèle possédé. */
OsintExecutionRecord *osint_execution_dao_find_by_identifier(
    OsintExecutionDao *dao, const char *identifier, GError **error
);
/**
 * @brief Liste les exécutions d'une sélection, de la plus récente à l'ancienne.
 *
 * @return Tableau possédé de OsintExecutionRecord, ou NULL en cas d'erreur.
 */
GPtrArray *osint_execution_dao_list_by_selection(
    OsintExecutionDao *dao, const char *selection_kind,
    const char *selection_identifier, GError **error
);
/**
 * @brief Liste les objets liés à une exécution sous forme de libellés possédés.
 *
 * Chaque libellé indique la nature, l'UUID et la disposition created/reused.
 *
 * @return Tableau possédé de chaînes, ou NULL en cas d'erreur.
 */
GPtrArray *osint_execution_dao_list_linked_objects(
    OsintExecutionDao *dao, const char *execution_identifier, GError **error
);
/** @brief Lie une entité créée ou réutilisée à une exécution. */
gboolean osint_execution_dao_link_entity(
    OsintExecutionDao *dao, const char *execution_identifier,
    const char *entity_identifier, const char *disposition, GError **error
);
/** @brief Lie une relation créée ou réutilisée à une exécution. */
gboolean osint_execution_dao_link_relation(
    OsintExecutionDao *dao, const char *execution_identifier,
    const char *relation_identifier, const char *disposition, GError **error
);

G_END_DECLS
#endif
