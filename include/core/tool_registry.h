/******************************************************************************
 * @file tool_registry.h
 * @brief Registre des outils et dépendances externes.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TOOL_REGISTRY_H
#define LABFY_INVESTIGATION_TOOL_REGISTRY_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Importance d’un outil externe pour l’application.
 */
typedef enum
{
    /**
     * L’absence de l’outil ne bloque pas l’application.
     */
    TOOL_REQUIREMENT_OPTIONAL,

    /**
     * L’outil est indispensable à la fonctionnalité concernée.
     */
    TOOL_REQUIREMENT_REQUIRED
} ToolRequirement;

/**
 * @brief État de disponibilité d’un outil externe.
 */
typedef enum
{
    /**
     * L’outil n’a pas encore été recherché.
     */
    TOOL_AVAILABILITY_UNKNOWN,

    /**
     * L’exécutable a été trouvé.
     */
    TOOL_AVAILABILITY_AVAILABLE,

    /**
     * L’exécutable n’a pas été trouvé.
     */
    TOOL_AVAILABILITY_MISSING
} ToolAvailability;

/**
 * @brief Erreurs produites par ToolRegistry.
 */
typedef enum
{
    /**
     * Un argument transmis à une fonction est invalide.
     */
    TOOL_REGISTRY_ERROR_INVALID_ARGUMENT,

    /**
     * Un outil possède le même identifiant qu’un outil déjà enregistré.
     */
    TOOL_REGISTRY_ERROR_DUPLICATE_IDENTIFIER,

    /**
     * L’outil demandé n’existe pas dans le registre.
     */
    TOOL_REGISTRY_ERROR_NOT_FOUND
} ToolRegistryError;

/**
 * @brief Domaine d’erreur du registre d’outils.
 */
#define TOOL_REGISTRY_ERROR \
    tool_registry_error_quark()

/**
 * @brief Registre opaque des outils externes.
 */
typedef struct ToolRegistry ToolRegistry;

/**
 * @brief Informations opaques décrivant un outil externe.
 */
typedef struct ToolInfo ToolInfo;

/**
 * @brief Retourne le domaine d’erreur du registre.
 *
 * @return Quark GLib du domaine d’erreur.
 */
GQuark tool_registry_error_quark(void);

/**
 * @brief Crée un registre d’outils vide.
 *
 * @return Nouveau registre, ou NULL en cas d’échec.
 */
ToolRegistry *tool_registry_new(void);

/**
 * @brief Libère un registre et tous les outils qu’il contient.
 *
 * @param tool_registry Registre à libérer, ou NULL.
 */
void tool_registry_free(
    ToolRegistry *tool_registry
);

/**
 * @brief Enregistre un nouvel outil externe.
 *
 * Le registre duplique toutes les chaînes reçues.
 *
 * L’outil commence avec :
 *
 * - une disponibilité inconnue ;
 * - aucun chemin résolu ;
 * - aucune version détectée.
 *
 * @param tool_registry Registre cible.
 * @param identifier Identifiant interne unique.
 * @param display_name Nom affiché à l’utilisateur.
 * @param executable_name Nom de l’exécutable recherché dans le PATH.
 * @param requirement Importance de la dépendance.
 * @param error Emplacement facultatif pour l’erreur.
 *
 * @return TRUE en cas de succès, sinon FALSE.
 */
gboolean tool_registry_register(
    ToolRegistry *tool_registry,
    const char *identifier,
    const char *display_name,
    const char *executable_name,
    ToolRequirement requirement,
    GError **error
);

/**
 * @brief Recherche tous les exécutables enregistrés dans le PATH.
 *
 * Chaque ancien chemin résolu est remplacé par le résultat de la nouvelle
 * détection.
 *
 * @param tool_registry Registre à actualiser.
 * @param error Emplacement facultatif pour l’erreur.
 *
 * @return TRUE lorsque le rafraîchissement a pu être effectué.
 */
gboolean tool_registry_refresh(
    ToolRegistry *tool_registry,
    GError **error
);

/**
 * @brief Retourne le nombre d’outils enregistrés.
 *
 * @param tool_registry Registre consulté.
 *
 * @return Nombre d’outils, ou zéro si le registre est NULL.
 */
gsize tool_registry_get_count(
    const ToolRegistry *tool_registry
);

/**
 * @brief Retourne un outil à partir de son index.
 *
 * Le pointeur retourné est emprunté. Il ne doit pas être libéré.
 *
 * @param tool_registry Registre consulté.
 * @param index Index de l’outil.
 *
 * @return Outil correspondant, ou NULL si l’index est invalide.
 */
const ToolInfo *tool_registry_get_tool(
    const ToolRegistry *tool_registry,
    gsize index
);

/**
 * @brief Recherche un outil à partir de son identifiant.
 *
 * Le pointeur retourné est emprunté. Il ne doit pas être libéré.
 *
 * @param tool_registry Registre consulté.
 * @param identifier Identifiant recherché.
 *
 * @return Outil correspondant, ou NULL s’il est absent.
 */
const ToolInfo *tool_registry_find(
    const ToolRegistry *tool_registry,
    const char *identifier
);

/**
 * @brief Mémorise la version détectée d’un outil.
 *
 * Une version NULL ou vide efface la version actuellement enregistrée.
 *
 * @param tool_registry Registre contenant l’outil.
 * @param identifier Identifiant de l’outil.
 * @param detected_version Version détectée, ou NULL.
 * @param error Emplacement facultatif pour l’erreur.
 *
 * @return TRUE en cas de succès, sinon FALSE.
 */
gboolean tool_registry_set_version(
    ToolRegistry *tool_registry,
    const char *identifier,
    const char *detected_version,
    GError **error
);

/**
 * @brief Indique si une dépendance obligatoire est explicitement absente.
 *
 * Les outils encore dans l’état UNKNOWN ne sont pas considérés ici comme
 * explicitement absents.
 *
 * @param tool_registry Registre consulté.
 *
 * @return TRUE si au moins un outil obligatoire est MISSING.
 */
gboolean tool_registry_has_missing_required_tools(
    const ToolRegistry *tool_registry
);

/**
 * @brief Indique si toutes les dépendances obligatoires sont disponibles.
 *
 * Un outil obligatoire dans l’état UNKNOWN empêche cette fonction de
 * retourner TRUE.
 *
 * @param tool_registry Registre consulté.
 *
 * @return TRUE si tous les outils obligatoires sont AVAILABLE.
 */
gboolean tool_registry_all_required_tools_available(
    const ToolRegistry *tool_registry
);

/**
 * @brief Retourne l’identifiant interne d’un outil.
 *
 * La chaîne retournée est empruntée.
 *
 * @param tool_info Outil consulté.
 *
 * @return Identifiant, ou NULL.
 */
const char *tool_info_get_identifier(
    const ToolInfo *tool_info
);

/**
 * @brief Retourne le nom affiché d’un outil.
 *
 * La chaîne retournée est empruntée.
 *
 * @param tool_info Outil consulté.
 *
 * @return Nom affiché, ou NULL.
 */
const char *tool_info_get_display_name(
    const ToolInfo *tool_info
);

/**
 * @brief Retourne le nom de l’exécutable d’un outil.
 *
 * La chaîne retournée est empruntée.
 *
 * @param tool_info Outil consulté.
 *
 * @return Nom de l’exécutable, ou NULL.
 */
const char *tool_info_get_executable_name(
    const ToolInfo *tool_info
);

/**
 * @brief Retourne l’importance d’un outil.
 *
 * @param tool_info Outil consulté.
 *
 * @return Importance de l’outil.
 */
ToolRequirement tool_info_get_requirement(
    const ToolInfo *tool_info
);

/**
 * @brief Retourne l’état de disponibilité d’un outil.
 *
 * @param tool_info Outil consulté.
 *
 * @return État de disponibilité.
 */
ToolAvailability tool_info_get_availability(
    const ToolInfo *tool_info
);

/**
 * @brief Retourne le chemin absolu détecté d’un outil.
 *
 * La chaîne retournée est empruntée.
 *
 * @param tool_info Outil consulté.
 *
 * @return Chemin détecté, ou NULL.
 */
const char *tool_info_get_resolved_path(
    const ToolInfo *tool_info
);

/**
 * @brief Retourne la version détectée d’un outil.
 *
 * La chaîne retournée est empruntée.
 *
 * @param tool_info Outil consulté.
 *
 * @return Version détectée, ou NULL.
 */
const char *tool_info_get_detected_version(
    const ToolInfo *tool_info
);

G_END_DECLS

#endif
