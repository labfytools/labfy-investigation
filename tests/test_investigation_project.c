/******************************************************************************
 * @file test_investigation_project.c
 * @brief Tests d'intégration du module InvestigationProject.
 ******************************************************************************/

#include "core/investigation_project.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Liste des dossiers attendus dans une enquête.
 *
 * Cette liste doit rester cohérente avec la structure créée par
 * investigation_project_create().
 */
static const char *const expected_directories[] =
{
    "00_BaseDeDonnees",

    "01_Preuves_Originales",
    "01_Preuves_Originales/Captures_Ecran",
    "01_Preuves_Originales/Conversations",
    "01_Preuves_Originales/Documents",
    "01_Preuves_Originales/Emails",
    "01_Preuves_Originales/Photos",
    "01_Preuves_Originales/Videos",

    "02_Preuves_Traitees",
    "02_Preuves_Traitees/Annotations",
    "02_Preuves_Traitees/Extractions",
    "02_Preuves_Traitees/OCR",
    "02_Preuves_Traitees/Redactions",

    "03_Chronologie",

    "04_Entites",
    "04_Entites/Adresses_Email",
    "04_Entites/Comptes_Bancaires",
    "04_Entites/Comptes_Facebook",
    "04_Entites/Comptes_Instagram",
    "04_Entites/Documents_Identite",
    "04_Entites/IBAN",
    "04_Entites/Personnes",
    "04_Entites/Pseudonymes",
    "04_Entites/Telephones",
    "04_Entites/Autres",

    "05_Rapports",
    "06_Exports",
    "07_Notes",
    "08_Sources",
    "09_Hash"
};

#define EXPECTED_DIRECTORY_COUNT \
    G_N_ELEMENTS(expected_directories)

/**
 * @brief Supprime récursivement un fichier ou un dossier de test.
 *
 * Cette fonction est réservée au nettoyage des répertoires temporaires créés
 * par les tests.
 *
 * @param path Chemin à supprimer.
 *
 * @return TRUE si le chemin a été supprimé ou n'existait pas, sinon FALSE.
 */
static gboolean test_remove_path_recursively(
    const char *path
)
{
    GDir *directory = NULL;
    const char *entry_name = NULL;
    GError *error = NULL;

    if (path == NULL)
    {
        return FALSE;
    }

    if (!g_file_test(path, G_FILE_TEST_EXISTS))
    {
        return TRUE;
    }

    if (!g_file_test(path, G_FILE_TEST_IS_DIR))
    {
        return g_remove(path) == 0;
    }

    directory = g_dir_open(
        path,
        0,
        &error
    );

    if (directory == NULL)
    {
        if (error != NULL)
        {
            g_printerr(
                "Impossible d'ouvrir '%s' pendant le nettoyage : %s\n",
                path,
                error->message
            );

            g_clear_error(&error);
        }

        return FALSE;
    }

    while ((entry_name = g_dir_read_name(directory)) != NULL)
    {
        char *entry_path = NULL;
        gboolean removal_success = FALSE;

        entry_path = g_build_filename(
            path,
            entry_name,
            NULL
        );

        assert(entry_path != NULL);

        removal_success = test_remove_path_recursively(
            entry_path
        );

        g_free(entry_path);

        if (!removal_success)
        {
            g_dir_close(directory);
            return FALSE;
        }
    }

    g_dir_close(directory);

    return g_rmdir(path) == 0;
}

/**
 * @brief Vérifie qu'un chemin désigne un dossier existant.
 */
static void test_assert_directory_exists(
    const char *root_path,
    const char *relative_path
)
{
    char *directory_path = NULL;

    assert(root_path != NULL);
    assert(relative_path != NULL);

    directory_path = g_build_filename(
        root_path,
        relative_path,
        NULL
    );

    assert(directory_path != NULL);
    assert(
        g_file_test(
            directory_path,
            G_FILE_TEST_IS_DIR
        )
    );

    g_free(directory_path);
}

/**
 * @brief Vérifie la création complète d'une enquête.
 */
static void test_create_valid_investigation(void)
{
    char *temporary_parent = NULL;
    char *investigation_path = NULL;
    char *expected_path = NULL;
    char *database_path = NULL;
    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-project-test-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    investigation_path = investigation_project_create(
        temporary_parent,
        "Enquete_Test"
    );

    assert(investigation_path != NULL);

    expected_path = g_build_filename(
        temporary_parent,
        "Enquete_Test",
        NULL
    );

    assert(expected_path != NULL);

    /*
     * Le chemin retourné doit être exactement celui de la nouvelle enquête.
     */
    assert(strcmp(investigation_path, expected_path) == 0);

    /*
     * Le dossier racine doit exister.
     */
    assert(
        g_file_test(
            investigation_path,
            G_FILE_TEST_IS_DIR
        )
    );

    /*
     * Vérification de tous les dossiers attendus.
     */
    for (gsize index = 0;
         index < EXPECTED_DIRECTORY_COUNT;
         ++index)
    {
        test_assert_directory_exists(
            investigation_path,
            expected_directories[index]
        );
    }

    database_path = g_build_filename(
        investigation_path,
        "00_BaseDeDonnees",
        "Enquete.sqlite",
        NULL
    );

    assert(database_path != NULL);

    /*
     * Enquete.sqlite doit exister et être un fichier régulier.
     */
    assert(
        g_file_test(
            database_path,
            G_FILE_TEST_EXISTS
        )
    );

    assert(
        g_file_test(
            database_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    /*
     * Une deuxième création au même endroit doit être refusée.
     */
    assert(
        investigation_project_create(
            temporary_parent,
            "Enquete_Test"
        ) == NULL
    );

    /*
     * Nettoyage complet.
     */
    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    assert(
        !g_file_test(
            temporary_parent,
            G_FILE_TEST_EXISTS
        )
    );

    g_free(database_path);
    g_free(expected_path);
    g_free(investigation_path);
    g_free(temporary_parent);
}

/**
 * @brief Vérifie les paramètres NULL et les chaînes vides.
 */
static void test_invalid_parameters(void)
{
    char *temporary_parent = NULL;
    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-invalid-test-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    assert(
        investigation_project_create(
            NULL,
            "Enquete"
        ) == NULL
    );

    assert(
        investigation_project_create(
            "",
            "Enquete"
        ) == NULL
    );

    assert(
        investigation_project_create(
            temporary_parent,
            NULL
        ) == NULL
    );

    assert(
        investigation_project_create(
            temporary_parent,
            ""
        ) == NULL
    );

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(temporary_parent);
}

/**
 * @brief Vérifie le refus d'un nom contenant un séparateur de chemin.
 */
static void test_invalid_investigation_name(void)
{
    char *temporary_parent = NULL;
    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-name-test-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    assert(
        investigation_project_create(
            temporary_parent,
            "Enquetes/Test"
        ) == NULL
    );

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(temporary_parent);
}

/**
 * @brief Vérifie le refus d'un dossier parent inexistant.
 */
static void test_missing_parent_directory(void)
{
    assert(
        investigation_project_create(
            "/tmp/labfy-parent-directory-that-does-not-exist",
            "Enquete"
        ) == NULL
    );
}

/**
 * @brief Vérifie le refus d'un chemin parent représentant un fichier.
 */
static void test_parent_is_file(void)
{
    char *temporary_directory = NULL;
    char *temporary_file = NULL;
    GError *error = NULL;

    temporary_directory = g_dir_make_tmp(
        "labfy-investigation-parent-file-test-XXXXXX",
        &error
    );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    temporary_file = g_build_filename(
        temporary_directory,
        "parent.txt",
        NULL
    );

    assert(temporary_file != NULL);

    assert(
        g_file_set_contents(
            temporary_file,
            "test\n",
            -1,
            &error
        )
    );

    assert(error == NULL);

    assert(
        investigation_project_create(
            temporary_file,
            "Enquete"
        ) == NULL
    );

    assert(
        test_remove_path_recursively(
            temporary_directory
        )
    );

    g_free(temporary_file);
    g_free(temporary_directory);
}

static void test_validate_created_investigation(void)
{
    char *temporary_parent = NULL;
    char *investigation_path = NULL;
    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-validation-test-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    investigation_path = investigation_project_create(
        temporary_parent,
        "Enquete_Valide"
    );

    assert(investigation_path != NULL);

    assert(
        investigation_project_validate(
            investigation_path
        )
    );

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(investigation_path);
    g_free(temporary_parent);
}

static void test_validate_invalid_paths(void)
{
    char *temporary_directory = NULL;
    char *temporary_file = NULL;
    GError *error = NULL;

    assert(!investigation_project_validate(NULL));
    assert(!investigation_project_validate(""));
    assert(
        !investigation_project_validate(
            "/tmp/labfy-investigation-does-not-exist"
        )
    );

    temporary_directory = g_dir_make_tmp(
        "labfy-investigation-validation-file-test-XXXXXX",
        &error
    );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    temporary_file = g_build_filename(
        temporary_directory,
        "not-an-investigation.txt",
        NULL
    );

    assert(temporary_file != NULL);

    assert(
        g_file_set_contents(
            temporary_file,
            "test\n",
            -1,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !investigation_project_validate(
            temporary_file
        )
    );

    assert(
        test_remove_path_recursively(
            temporary_directory
        )
    );

    g_free(temporary_file);
    g_free(temporary_directory);
}

static void test_validate_missing_database(void)
{
    char *temporary_parent = NULL;
    char *investigation_path = NULL;
    char *database_path = NULL;
    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-missing-database-test-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    investigation_path = investigation_project_create(
        temporary_parent,
        "Enquete_Sans_Base"
    );

    assert(investigation_path != NULL);

    database_path = g_build_filename(
        investigation_path,
        "00_BaseDeDonnees",
        "Enquete.sqlite",
        NULL
    );

    assert(database_path != NULL);
    assert(g_remove(database_path) == 0);

    assert(
        !investigation_project_validate(
            investigation_path
        )
    );

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(database_path);
    g_free(investigation_path);
    g_free(temporary_parent);
}

static void test_validate_missing_directory(void)
{
    char *temporary_parent = NULL;
    char *investigation_path = NULL;
    char *directory_path = NULL;
    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-missing-directory-test-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    investigation_path = investigation_project_create(
        temporary_parent,
        "Enquete_Incomplete"
    );

    assert(investigation_path != NULL);

    directory_path = g_build_filename(
        investigation_path,
        "05_Rapports",
        NULL
    );

    assert(directory_path != NULL);
    assert(g_rmdir(directory_path) == 0);

    assert(
        !investigation_project_validate(
            investigation_path
        )
    );

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(directory_path);
    g_free(investigation_path);
    g_free(temporary_parent);
}

static void test_validate_directory_replaced_by_file(void)
{
    char *temporary_parent = NULL;
    char *investigation_path = NULL;
    char *directory_path = NULL;
    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-wrong-type-test-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    investigation_path = investigation_project_create(
        temporary_parent,
        "Enquete_Mauvais_Type"
    );

    assert(investigation_path != NULL);

    directory_path = g_build_filename(
        investigation_path,
        "07_Notes",
        NULL
    );

    assert(directory_path != NULL);
    assert(g_rmdir(directory_path) == 0);

    assert(
        g_file_set_contents(
            directory_path,
            "faux dossier\n",
            -1,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !investigation_project_validate(
            investigation_path
        )
    );

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(directory_path);
    g_free(investigation_path);
    g_free(temporary_parent);
}

int main(void)
{
    test_create_valid_investigation();
    test_invalid_parameters();
    test_invalid_investigation_name();
    test_missing_parent_directory();
    test_parent_is_file();

    test_validate_created_investigation();
    test_validate_invalid_paths();
    test_validate_missing_database();
    test_validate_missing_directory();
    test_validate_directory_replaced_by_file();

    printf(
        "InvestigationProject : tous les tests sont valides.\n"
    );

    return 0;
}
