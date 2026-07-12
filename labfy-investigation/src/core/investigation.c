/******************************************************************************
 * @file investigation.c
 * @brief Implémentation du module représentant une enquête.
 ******************************************************************************/

#include "core/investigation.h"

#include <stdbool.h>

#include <glib.h>

/**
 * @brief Nom du dossier contenant la base de données d'une enquête.
 */
#define INVESTIGATION_DATABASE_DIRECTORY "00_BaseDeDonnees"

/**
 * @brief Nom du fichier SQLite d'une enquête.
 */
#define INVESTIGATION_DATABASE_FILENAME "Enquete.sqlite"

/**
 * @struct Investigation
 * @brief Représentation interne d'une enquête.
 *
 * Cette structure est privée au module. Les autres fichiers connaissent
 * uniquement le type opaque déclaré dans investigation.h.
 */
struct Investigation
{
    char *root_path;
    char *database_path;
};

/**
 * @brief Vérifie qu'un chemin peut servir de dossier racine à une enquête.
 *
 * @param root_path Chemin à vérifier.
 *
 * @return true si le chemin désigne un dossier existant, sinon false.
 */
static bool investigation_root_path_is_valid(const char *root_path)
{
    if (root_path == NULL)
    {
        return false;
    }

    if (root_path[0] == '\0')
    {
        return false;
    }

    if (!g_file_test(root_path, G_FILE_TEST_IS_DIR))
    {
        return false;
    }

    return true;
}

Investigation *investigation_new(const char *root_path)
{
    Investigation *investigation = NULL;

    if (!investigation_root_path_is_valid(root_path))
    {
        return NULL;
    }

    investigation = g_new0(Investigation, 1);

    investigation->root_path = g_canonicalize_filename(root_path, NULL);

    if (investigation->root_path == NULL)
    {
        investigation_free(investigation);
        return NULL;
    }

    investigation->database_path = g_build_filename(
        investigation->root_path,
        INVESTIGATION_DATABASE_DIRECTORY,
        INVESTIGATION_DATABASE_FILENAME,
        NULL
    );

    if (investigation->database_path == NULL)
    {
        investigation_free(investigation);
        return NULL;
    }

    return investigation;
}

void investigation_free(Investigation *investigation)
{
    if (investigation == NULL)
    {
        return;
    }

    g_free(investigation->database_path);
    g_free(investigation->root_path);
    g_free(investigation);
}

const char *investigation_get_root_path(
    const Investigation *investigation
)
{
    if (investigation == NULL)
    {
        return NULL;
    }

    return investigation->root_path;
}

const char *investigation_get_database_path(
    const Investigation *investigation
)
{
    if (investigation == NULL)
    {
        return NULL;
    }

    return investigation->database_path;
}
