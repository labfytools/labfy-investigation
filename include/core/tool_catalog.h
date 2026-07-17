/******************************************************************************
 * @file tool_catalog.h
 * @brief Catalogue statique des outils externes connus.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TOOL_CATALOG_H
#define LABFY_INVESTIGATION_TOOL_CATALOG_H

#include "core/tool_registry.h"

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Erreurs produites par ToolCatalog.
 */
typedef enum
{
    /**
     * Un argument transmis au module est invalide.
     */
    TOOL_CATALOG_ERROR_INVALID_ARGUMENT,

    /**
     * L'identifiant demandé n'existe pas dans le catalogue.
     */
    TOOL_CATALOG_ERROR_ENTRY_NOT_FOUND,

    /**
     * Le catalogue n'a pas pu être enregistré dans le registre.
     */
    TOOL_CATALOG_ERROR_REGISTRATION,

    /**
     * L'outil existe dans le catalogue, mais pas dans le registre.
     */
    TOOL_CATALOG_ERROR_TOOL_NOT_REGISTERED,

    /**
     * La disponibilité de l'outil n'a pas encore été vérifiée.
     */
    TOOL_CATALOG_ERROR_TOOL_NOT_CHECKED,

    /**
     * L'outil a été vérifié, mais son exécutable est absent.
     */
    TOOL_CATALOG_ERROR_TOOL_MISSING,

    /**
     * L'outil est disponible, mais son état interne est incohérent.
     */
    TOOL_CATALOG_ERROR_INVALID_TOOL_STATE,

    /**
     * ToolProcess n'a pas pu exécuter ou interroger l'outil.
     */
    TOOL_CATALOG_ERROR_PROCESS,

    /**
     * La commande de version s'est terminée avec un code non nul
     * ou à la suite d'un signal.
     */
    TOOL_CATALOG_ERROR_VERSION_COMMAND,

    /**
     * La commande n'a produit aucune version exploitable.
     */
    TOOL_CATALOG_ERROR_VERSION_OUTPUT
} ToolCatalogError;

/**
 * @brief Domaine d'erreur du catalogue d'outils.
 */
#define TOOL_CATALOG_ERROR \
    tool_catalog_error_quark()

/**
 * @brief Entrée opaque et immuable du catalogue.
 *
 * Les entrées sont conservées dans une table statique interne.
 * Elles ne doivent jamais être libérées par l'appelant.
 */
typedef struct ToolCatalogEntry ToolCatalogEntry;

/**
 * @brief Retourne le domaine d'erreur du catalogue.
 *
 * @return Quark GLib du domaine d'erreur.
 */
GQuark tool_catalog_error_quark(void);

/**
 * @brief Retourne le nombre d'outils connus du catalogue.
 *
 * @return Nombre d'entrées statiques.
 */
gsize tool_catalog_get_count(void);

/**
 * @brief Retourne une entrée à partir de son index.
 *
 * Le pointeur retourné est emprunté et statique.
 *
 * @param index Index de l'entrée.
 *
 * @return Entrée correspondante, ou NULL si l'index est invalide.
 */
const ToolCatalogEntry *tool_catalog_get_entry(
    gsize index
);

/**
 * @brief Recherche une entrée par son identifiant interne.
 *
 * Le pointeur retourné est emprunté et statique.
 *
 * @param identifier Identifiant recherché.
 *
 * @return Entrée correspondante, ou NULL si elle n'existe pas.
 */
const ToolCatalogEntry *tool_catalog_find(
    const char *identifier
);

/**
 * @brief Retourne l'identifiant interne d'une entrée.
 *
 * La chaîne retournée est empruntée et statique.
 *
 * @param entry Entrée consultée.
 *
 * @return Identifiant de l'outil, ou NULL.
 */
const char *tool_catalog_entry_get_identifier(
    const ToolCatalogEntry *entry
);

/**
 * @brief Retourne le nom affiché d'une entrée.
 *
 * La chaîne retournée est empruntée et statique.
 *
 * @param entry Entrée consultée.
 *
 * @return Nom affiché, ou NULL.
 */
const char *tool_catalog_entry_get_display_name(
    const ToolCatalogEntry *entry
);

/**
 * @brief Retourne le nom de l'exécutable recherché dans le PATH.
 *
 * La chaîne retournée est empruntée et statique.
 *
 * @param entry Entrée consultée.
 *
 * @return Nom de l'exécutable, ou NULL.
 */
const char *tool_catalog_entry_get_executable_name(
    const ToolCatalogEntry *entry
);

/**
 * @brief Retourne l'importance de la dépendance.
 *
 * @param entry Entrée consultée.
 *
 * @return Importance de l'outil.
 */
ToolRequirement tool_catalog_entry_get_requirement(
    const ToolCatalogEntry *entry
);

/**
 * @brief Retourne le nombre d'arguments de la commande de version.
 *
 * Le chemin de l'exécutable n'est pas compté.
 *
 * @param entry Entrée consultée.
 *
 * @return Nombre d'arguments.
 */
gsize tool_catalog_entry_get_version_argument_count(
    const ToolCatalogEntry *entry
);

/**
 * @brief Retourne un argument de la commande de version.
 *
 * La chaîne retournée est empruntée et statique.
 *
 * @param entry Entrée consultée.
 * @param index Index de l'argument.
 *
 * @return Argument correspondant, ou NULL si l'index est invalide.
 */
const char *tool_catalog_entry_get_version_argument(
    const ToolCatalogEntry *entry,
    gsize index
);

/**
 * @brief Enregistre toutes les entrées du catalogue dans un registre.
 *
 * Avant toute insertion, la fonction vérifie qu'aucun identifiant du
 * catalogue n'existe déjà dans le registre.
 *
 * En cas de doublon, aucune entrée n'est ajoutée.
 *
 * @param tool_registry Registre cible.
 * @param error Emplacement facultatif pour l'erreur.
 *
 * @return TRUE si toutes les entrées ont été enregistrées.
 */
gboolean tool_catalog_register_defaults(
    ToolRegistry *tool_registry,
    GError **error
);

/**
 * @brief Détecte la version d'un outil disponible.
 *
 * La fonction :
 *
 * - vérifie que l'outil existe dans le catalogue et le registre ;
 * - vérifie que son exécutable est disponible ;
 * - exécute sa commande de version avec ToolProcess ;
 * - sélectionne la première ligne non vide de stdout ou stderr ;
 * - retourne une chaîne UTF-8 normalisée.
 *
 * La fonction ne modifie pas la version conservée dans ToolRegistry.
 *
 * En cas d'annulation, l'erreur retournée appartient au domaine
 * G_IO_ERROR avec le code G_IO_ERROR_CANCELLED.
 *
 * @param tool_registry Registre contenant l'outil détecté.
 * @param identifier Identifiant de l'entrée du catalogue.
 * @param cancellable Objet d'annulation facultatif.
 * @param out_version Emplacement recevant une nouvelle chaîne.
 * @param error Emplacement facultatif pour l'erreur.
 *
 * @return TRUE si une version exploitable a été détectée.
 */
gboolean tool_catalog_detect_version(
    const ToolRegistry *tool_registry,
    const char *identifier,
    GCancellable *cancellable,
    char **out_version,
    GError **error
);

G_END_DECLS

#endif
