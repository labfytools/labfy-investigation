/******************************************************************************
 * @file test_social_platform.c
 * @brief Tests de la correspondance visuelle des plateformes sociales.
 ******************************************************************************/

#include "models/social_platform.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/** @brief Vérifie tous les types sociaux persistés et le cas générique. */
static void test_social_platform_mapping(void)
{
    assert(social_platform_from_entity_type("tiktok_account") ==
        SOCIAL_PLATFORM_TIKTOK);
    assert(social_platform_from_entity_type("instagram_account") ==
        SOCIAL_PLATFORM_INSTAGRAM);
    assert(social_platform_from_entity_type("facebook_account") ==
        SOCIAL_PLATFORM_FACEBOOK);
    assert(social_platform_from_entity_type("x_account") == SOCIAL_PLATFORM_X);
    assert(social_platform_from_entity_type("telegram_account") ==
        SOCIAL_PLATFORM_TELEGRAM);
    assert(social_platform_from_entity_type("social_account") ==
        SOCIAL_PLATFORM_OTHER);
    assert(social_platform_from_entity_type("person") == SOCIAL_PLATFORM_NONE);
    assert(social_platform_from_entity_type(NULL) == SOCIAL_PLATFORM_NONE);
    assert(strcmp(social_platform_get_label(SOCIAL_PLATFORM_INSTAGRAM),
        "Instagram") == 0);
    assert(social_platform_get_label(SOCIAL_PLATFORM_NONE) == NULL);
}

int main(void)
{
    test_social_platform_mapping();
    puts("SocialPlatform : tous les tests sont valides.");
    return 0;
}
