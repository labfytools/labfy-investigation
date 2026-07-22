/******************************************************************************
 * @file social_platform.c
 * @brief Identification visuelle des plateformes sociales du graphe.
 ******************************************************************************/

#include "models/social_platform.h"

SocialPlatform social_platform_from_entity_type(
    const char *entity_type_identifier
)
{
    if (g_strcmp0(entity_type_identifier, "tiktok_account") == 0)
        return SOCIAL_PLATFORM_TIKTOK;
    if (g_strcmp0(entity_type_identifier, "instagram_account") == 0)
        return SOCIAL_PLATFORM_INSTAGRAM;
    if (g_strcmp0(entity_type_identifier, "facebook_account") == 0)
        return SOCIAL_PLATFORM_FACEBOOK;
    if (g_strcmp0(entity_type_identifier, "x_account") == 0)
        return SOCIAL_PLATFORM_X;
    if (g_strcmp0(entity_type_identifier, "telegram_account") == 0)
        return SOCIAL_PLATFORM_TELEGRAM;
    if (g_strcmp0(entity_type_identifier, "social_account") == 0)
        return SOCIAL_PLATFORM_OTHER;
    return SOCIAL_PLATFORM_NONE;
}

const char *social_platform_get_label(
    SocialPlatform platform
)
{
    switch (platform)
    {
        case SOCIAL_PLATFORM_TIKTOK: return "TikTok";
        case SOCIAL_PLATFORM_INSTAGRAM: return "Instagram";
        case SOCIAL_PLATFORM_FACEBOOK: return "Facebook";
        case SOCIAL_PLATFORM_X: return "X";
        case SOCIAL_PLATFORM_TELEGRAM: return "Telegram";
        case SOCIAL_PLATFORM_OTHER: return "Réseau social";
        case SOCIAL_PLATFORM_NONE: return NULL;
    }
    return NULL;
}
