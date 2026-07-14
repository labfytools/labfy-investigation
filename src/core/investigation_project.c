/******************************************************************************
 * @file investigation_project.c
 * @brief Création et gestion de la structure d'un projet d'enquête.
 ******************************************************************************/

#include "core/investigation_project.h"

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Liste des dossiers relatifs composant une enquête.
 *
 * L'ordre est important : les dossiers parents doivent apparaître avant
 * leurs sous-dossiers.
 */
static const char *const investigation_project_directories[] =
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

/**
 * @brief Nombre de dossiers présents dans la table de création.
 */
#define INVESTIGATION_PROJECT_DIRECTORY_COUNT \
    G_N_ELEMENTS(investigation_project_directories)

/**
 * @brief Vérifie que les paramètres nécessaires à la création sont valides.
 *
 * @param parent_directory  Dossier dans lequel créer l'enquête.
 * @param investigation_name Nom de la nouvelle enquête.
 *
 * @return TRUE si les paramètres sont valides, sinon FALSE.
 */
static gboolean investigation_project_validate_create_parameters(
    const char *parent_directory,
    const char *investigation_name
)
{
    if (parent_directory == NULL ||
        parent_directory[0] == '\0')
    {
        return FALSE;
    }

    if (investigation_name == NULL ||
        investigation_name[0] == '\0')
    {
        return FALSE;
    }

    /*
     * Le nom doit représenter un seul composant de chemin.
     *
     * On refuse par exemple :
     *
     * Enquetes/Test
     */
    if (strchr(investigation_name, G_DIR_SEPARATOR) != NULL)
    {
        return FALSE;
    }

    if (!g_file_test(
            parent_directory,
            G_FILE_TEST_EXISTS
        ))
    {
        return FALSE;
    }

    if (!g_file_test(
            parent_directory,
            G_FILE_TEST_IS_DIR
        ))
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Supprime les fichiers et dossiers créés pendant une tentative.
 *
 * Les chemins sont parcourus dans l'ordre inverse afin de supprimer :
 *
 * 1. les fichiers ;
 * 2. les sous-dossiers ;
 * 3. le dossier racine.
 *
 * Cette fonction ne reçoit que des chemins créés par le module pendant
 * l'appel courant.
 *
 * @param created_paths Tableau contenant les chemins créés.
 */
static void investigation_project_cleanup_created_paths(
    GPtrArray *created_paths
)
{
    if (created_paths == NULL)
    {
        return;
    }

    for (gsize index = created_paths->len; index > 0; --index)
    {
        const char *path = NULL;

        path = g_ptr_array_index(
            created_paths,
            index - 1
        );

        if (path == NULL)
        {
            continue;
        }

        if (g_file_test(path, G_FILE_TEST_IS_DIR))
        {
            if (g_rmdir(path) != 0)
            {
                g_warning(
                    "Impossible de supprimer le dossier temporaire '%s' : %s",
                    path,
                    g_strerror(errno)
                );
            }
        }
        else if (g_file_test(path, G_FILE_TEST_EXISTS))
        {
            if (g_remove(path) != 0)
            {
                g_warning(
                    "Impossible de supprimer le fichier temporaire '%s' : %s",
                    path,
                    g_strerror(errno)
                );
            }
        }
    }
}

/**
 * @brief Crée un dossier et mémorise son chemin pour un éventuel nettoyage.
 *
 * La fonction prend possession de directory_path uniquement si la création
 * réussit et que le chemin est ajouté au tableau.
 *
 * @param directory_path Chemin alloué du dossier à créer.
 * @param created_paths  Tableau des éléments créés.
 *
 * @return TRUE si le dossier a été créé, sinon FALSE.
 */
static gboolean investigation_project_create_directory(
    char *directory_path,
    GPtrArray *created_paths
)
{
    if (directory_path == NULL || created_paths == NULL)
    {
        g_free(directory_path);
        return FALSE;
    }

    if (g_mkdir(directory_path, 0700) != 0)
    {
        g_warning(
            "Impossible de créer le dossier '%s' : %s",
            directory_path,
            g_strerror(errno)
        );

        g_free(directory_path);

        return FALSE;
    }

    /*
     * Le tableau devient propriétaire de directory_path.
     */
    g_ptr_array_add(
        created_paths,
        directory_path
    );

    return TRUE;
}

char *investigation_project_create(
    const char *parent_directory,
    const char *investigation_name
)
{
    char *investigation_path = NULL;
    char *directory_path = NULL;
    char *database_path = NULL;

    GPtrArray *created_paths = NULL;
    GError *error = NULL;

    if (!investigation_project_validate_create_parameters(
            parent_directory,
            investigation_name
        ))
    {
        return NULL;
    }

    investigation_path = g_build_filename(
        parent_directory,
        investigation_name,
        NULL
    );

    if (investigation_path == NULL)
    {
        return NULL;
    }

    /*
     * Une enquête existante ne doit jamais être écrasée.
     */
    if (g_file_test(
            investigation_path,
            G_FILE_TEST_EXISTS
        ))
    {
        g_free(investigation_path);
        return NULL;
    }

    /*
     * Le tableau conserve tous les chemins créés.
     *
     * Ses chaînes seront libérées automatiquement par g_ptr_array_free().
     */
    created_paths = g_ptr_array_new_with_free_func(
        g_free
    );

    if (created_paths == NULL)
    {
        g_free(investigation_path);
        return NULL;
    }

    /*
     * On crée d'abord le dossier racine.
     *
     * Une copie est placée dans created_paths, car investigation_path
     * doit être retourné au code appelant en cas de succès.
     */
    directory_path = g_strdup(investigation_path);

    if (!investigation_project_create_directory(
            directory_path,
            created_paths
        ))
    {
        g_ptr_array_free(created_paths, TRUE);
        g_free(investigation_path);

        return NULL;
    }

    /*
     * Création de toute l'arborescence déclarée dans la table.
     */
    for (gsize index = 0;
         index < INVESTIGATION_PROJECT_DIRECTORY_COUNT;
         ++index)
    {
        directory_path = g_build_filename(
            investigation_path,
            investigation_project_directories[index],
            NULL
        );

        if (!investigation_project_create_directory(
                directory_path,
                created_paths
            ))
        {
            investigation_project_cleanup_created_paths(
                created_paths
            );

            g_ptr_array_free(
                created_paths,
                TRUE
            );

            g_free(investigation_path);

            return NULL;
        }
    }

    database_path = g_build_filename(
        investigation_path,
        "00_BaseDeDonnees",
        "Enquete.sqlite",
        NULL
    );

    if (database_path == NULL)
    {
        investigation_project_cleanup_created_paths(
            created_paths
        );

        g_ptr_array_free(
            created_paths,
            TRUE
        );

        g_free(investigation_path);

        return NULL;
    }

    /*
     * Pour ce ticket, on crée seulement un fichier vide.
     *
     * Le schéma SQLite sera ajouté dans un prochain ticket.
     */
    if (!g_file_set_contents(
            database_path,
            "",
            0,
            &error
        ))
    {
        if (error != NULL)
        {
            g_warning(
                "Impossible de créer '%s' : %s",
                database_path,
                error->message
            );

            g_clear_error(&error);
        }

        g_free(database_path);

        investigation_project_cleanup_created_paths(
            created_paths
        );

        g_ptr_array_free(
            created_paths,
            TRUE
        );

        g_free(investigation_path);

        return NULL;
    }

    /*
     * Le fichier doit également être supprimé avant ses dossiers
     * si une évolution future ajoute une étape pouvant encore échouer.
     *
     * Le tableau devient propriétaire de database_path.
     */
    g_ptr_array_add(
        created_paths,
        database_path
    );

    /*
     * Tout est créé avec succès.
     *
     * On libère seulement le tableau et ses copies de chemins.
     * Les fichiers et dossiers restent sur le disque.
     */
    g_ptr_array_free(
        created_paths,
        TRUE
    );

    return investigation_path;
}
