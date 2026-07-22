/******************************************************************************
 * @file social_platform.h
 * @brief Identification visuelle des plateformes sociales du graphe.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_SOCIAL_PLATFORM_H
#define LABFY_INVESTIGATION_SOCIAL_PLATFORM_H

#include <glib.h>

G_BEGIN_DECLS

/** @brief Plateformes possédant un pictogramme dans le graphe. */
typedef enum
{
    SOCIAL_PLATFORM_NONE,
    SOCIAL_PLATFORM_TIKTOK,
    SOCIAL_PLATFORM_INSTAGRAM,
    SOCIAL_PLATFORM_FACEBOOK,
    SOCIAL_PLATFORM_X,
    SOCIAL_PLATFORM_TELEGRAM,
    SOCIAL_PLATFORM_OTHER
} SocialPlatform;

/**
 * @brief Déduit la plateforme depuis le code du type d'entité.
 * @param entity_type_identifier Code métier du type d'entité.
 * @return Plateforme correspondante, ou SOCIAL_PLATFORM_NONE.
 */
SocialPlatform social_platform_from_entity_type(
    const char *entity_type_identifier
);

/**
 * @brief Retourne le libellé français affichable d'une plateforme.
 * @param platform Plateforme à décrire.
 * @return Chaîne statique, ou NULL pour SOCIAL_PLATFORM_NONE.
 */
const char *social_platform_get_label(
    SocialPlatform platform
);

G_END_DECLS

#endif
