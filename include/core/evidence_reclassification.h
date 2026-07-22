/******************************************************************************
 * @file evidence_reclassification.h
 * @brief Reclassement cohérent d'une preuve sans modifier son contenu.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_RECLASSIFICATION_H
#define LABFY_INVESTIGATION_EVIDENCE_RECLASSIFICATION_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Déplace une preuve et met à jour ses métadonnées avec retour arrière.
 *
 * L'intégrité est contrôlée avant et après le déplacement. Le contenu,
 * l'UUID, le nom interne, la taille, l'empreinte et la date d'import ne sont
 * jamais modifiés.
 *
 * @param database Connexion SQLite ouverte.
 * @param investigation_root Racine de l'enquête.
 * @param evidence_identifier UUID de la preuve.
 * @param type_identifier Nouveau type métier.
 * @param relative_directory Dossier relatif de destination.
 * @param source Nouvelle source, ou NULL.
 * @param description Nouvelle description, ou NULL.
 * @param error Adresse recevant une éventuelle erreur.
 * @return TRUE si le reclassement complet réussit.
 */
gboolean evidence_reclassification_apply(
    Database *database,
    const char *investigation_root,
    const char *evidence_identifier,
    const char *type_identifier,
    const char *relative_directory,
    const char *source,
    const char *description,
    GError **error
);

G_END_DECLS
#endif
