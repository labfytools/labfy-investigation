/******************************************************************************
 * @file investigation_project.c
 * @brief Création et gestion de la structure d'un projet d'enquête.
 ******************************************************************************/

#include "core/investigation_project.h"
#include "database/database.h"

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Nom du dossier contenant la base de données.
 */
#define INVESTIGATION_PROJECT_DATABASE_DIRECTORY \
    "00_BaseDeDonnees"

/**
 * @brief Nom du fichier SQLite d'une enquête.
 */
#define INVESTIGATION_PROJECT_DATABASE_FILENAME \
    "Enquete.sqlite"

/**
 * @brief Représentation privée des chemins d'un projet d'enquête.
 */
struct InvestigationProject
{
    char *root_path;
    char *database_path;
};

/**
 * @brief Type d'un élément de la structure d'une enquête.
 */
typedef enum
{
    INVESTIGATION_PROJECT_DIRECTORY,
    INVESTIGATION_PROJECT_FILE
} InvestigationProjectEntryType;

/**
 * @brief Décrit un élément attendu dans une enquête.
 */
typedef struct
{
    /**
     * Chemin relatif depuis la racine de l'enquête.
     */
    const char *relative_path;

    /**
     * Nature de l'élément.
     */
    InvestigationProjectEntryType type;

} InvestigationProjectEntry;

/**
 * @brief Liste des dossiers relatifs composant une enquête.
 *
 * L'ordre est important : les dossiers parents doivent apparaître avant
 * leurs sous-dossiers.
 */
static const InvestigationProjectEntry
investigation_project_entries[] =
{
    { "00_BaseDeDonnees", INVESTIGATION_PROJECT_DIRECTORY },
    { "00_BaseDeDonnees/Enquete.sqlite", INVESTIGATION_PROJECT_FILE },

    { "01_Preuves_Originales", INVESTIGATION_PROJECT_DIRECTORY },
    { "01_Preuves_Originales/Captures_Ecran", INVESTIGATION_PROJECT_DIRECTORY },
    { "01_Preuves_Originales/Conversations", INVESTIGATION_PROJECT_DIRECTORY },
    { "01_Preuves_Originales/Documents", INVESTIGATION_PROJECT_DIRECTORY },
    { "01_Preuves_Originales/Emails", INVESTIGATION_PROJECT_DIRECTORY },
    { "01_Preuves_Originales/Photos", INVESTIGATION_PROJECT_DIRECTORY },
    { "01_Preuves_Originales/Videos", INVESTIGATION_PROJECT_DIRECTORY },

    { "02_Preuves_Traitees", INVESTIGATION_PROJECT_DIRECTORY },
    { "02_Preuves_Traitees/Annotations", INVESTIGATION_PROJECT_DIRECTORY },
    { "02_Preuves_Traitees/Extractions", INVESTIGATION_PROJECT_DIRECTORY },
    { "02_Preuves_Traitees/OCR", INVESTIGATION_PROJECT_DIRECTORY },
    { "02_Preuves_Traitees/Redactions", INVESTIGATION_PROJECT_DIRECTORY },

    { "03_Chronologie", INVESTIGATION_PROJECT_DIRECTORY },

    { "04_Entites", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Adresses_Email", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Comptes_Bancaires", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Comptes_Facebook", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Comptes_Instagram", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Documents_Identite", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/IBAN", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Personnes", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Pseudonymes", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Telephones", INVESTIGATION_PROJECT_DIRECTORY },
    { "04_Entites/Autres", INVESTIGATION_PROJECT_DIRECTORY },

    { "05_Rapports", INVESTIGATION_PROJECT_DIRECTORY },
    { "06_Exports", INVESTIGATION_PROJECT_DIRECTORY },
    { "07_Notes", INVESTIGATION_PROJECT_DIRECTORY },
    { "08_Sources", INVESTIGATION_PROJECT_DIRECTORY },
    { "09_Hash", INVESTIGATION_PROJECT_DIRECTORY }
};

/**
 * @brief Nombre de dossiers présents dans la table de création.
 */
#define INVESTIGATION_PROJECT_ENTRY_COUNT \
    G_N_ELEMENTS(investigation_project_entries)

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

/**
 * @brief Vérifie qu'un élément existe avec le type attendu.
 *
 * @param investigation_root Racine de l'enquête.
 * @param entry Élément à vérifier.
 *
 * @return TRUE si l'élément est valide.
 */
static gboolean investigation_project_validate_entry(
    const char *investigation_root,
    const InvestigationProjectEntry *entry
)
{
    char *path = NULL;
    gboolean valid = FALSE;

    if (investigation_root == NULL ||
        entry == NULL)
    {
        return FALSE;
    }

    path = g_build_filename(
        investigation_root,
        entry->relative_path,
        NULL
    );

    if (path == NULL)
    {
        return FALSE;
    }

    switch (entry->type)
    {
        case INVESTIGATION_PROJECT_DIRECTORY:

            valid = g_file_test(
                path,
                G_FILE_TEST_IS_DIR
            );

            break;

        case INVESTIGATION_PROJECT_FILE:

            valid = g_file_test(
                path,
                G_FILE_TEST_IS_REGULAR
            );

            break;

        default:

            valid = FALSE;
            break;
    }

    g_free(path);

    return valid;
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
        index < INVESTIGATION_PROJECT_ENTRY_COUNT;
        ++index)
    {
        const InvestigationProjectEntry *entry = NULL;

        entry = &investigation_project_entries[index];

        if (entry->type != INVESTIGATION_PROJECT_DIRECTORY)
        {
            continue;
        }

        directory_path = g_build_filename(
            investigation_path,
            entry->relative_path,
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
        INVESTIGATION_PROJECT_DATABASE_DIRECTORY,
        INVESTIGATION_PROJECT_DATABASE_FILENAME,
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
     * Le chemin est enregistré avant l'initialisation.
     *
     * Ainsi, si database_initialize() crée le fichier puis échoue,
     * le nettoyage pourra supprimer ce fichier avant ses dossiers parents.
     *
     * Le tableau devient propriétaire de database_path.
     */
    g_ptr_array_add(
        created_paths,
        database_path
    );

    if (!database_initialize(
            database_path,
            investigation_name,
            investigation_path
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

    /*
     * Tout est créé avec succès.
     *
     * On libère uniquement le tableau et ses chaînes.
     * Les fichiers et dossiers restent présents sur le disque.
     */
    g_ptr_array_free(
        created_paths,
        TRUE
    );

    return investigation_path;
}

bool investigation_project_validate(
    const char *investigation_path
)
{
    if (investigation_path == NULL)
    {
        return false;
    }

    if (investigation_path[0] == '\0')
    {
        return false;
    }

    if (!g_file_test(
            investigation_path,
            G_FILE_TEST_EXISTS))
    {
        return false;
    }

    if (!g_file_test(
            investigation_path,
            G_FILE_TEST_IS_DIR))
    {
        return false;
    }

    for (gsize index = 0;
         index < INVESTIGATION_PROJECT_ENTRY_COUNT;
         ++index)
    {
        const InvestigationProjectEntry *entry = NULL;

        entry = &investigation_project_entries[index];

        if (!investigation_project_validate_entry(
                investigation_path,
                entry))
        {
            return false;
        }
    }

    return true;
}

InvestigationProject *investigation_project_open(
    const char *investigation_root_path
)
{
    InvestigationProject *project = NULL;
    char *canonical_root_path = NULL;

    if (investigation_root_path == NULL ||
        investigation_root_path[0] == '\0')
    {
        return NULL;
    }

    canonical_root_path = g_canonicalize_filename(
        investigation_root_path,
        NULL
    );

    if (canonical_root_path == NULL)
    {
        return NULL;
    }

    /*
     * Le contexte représente une enquête existante.
     *
     * On vérifie seulement ici que sa racine est un dossier.
     * La présence de la base SQLite sera contrôlée séparément par
     * InvestigationSession afin de produire une erreur précise.
     */
    if (!g_file_test(
            canonical_root_path,
            G_FILE_TEST_IS_DIR
        ))
    {
        g_free(canonical_root_path);
        return NULL;
    }

    project = g_try_new0(
        InvestigationProject,
        1
    );

    if (project == NULL)
    {
        g_free(canonical_root_path);
        return NULL;
    }

    project->root_path = canonical_root_path;

    project->database_path = g_build_filename(
        project->root_path,
        INVESTIGATION_PROJECT_DATABASE_DIRECTORY,
        INVESTIGATION_PROJECT_DATABASE_FILENAME,
        NULL
    );

    if (project->database_path == NULL)
    {
        investigation_project_free(project);
        return NULL;
    }

    return project;
}

void investigation_project_free(
    InvestigationProject *project
)
{
    if (project == NULL)
    {
        return;
    }

    g_free(project->database_path);
    g_free(project->root_path);
    g_free(project);
}

const char *investigation_project_get_root_path(
    const InvestigationProject *project
)
{
    if (project == NULL)
    {
        return NULL;
    }

    return project->root_path;
}

const char *investigation_project_get_database_path(
    const InvestigationProject *project
)
{
    if (project == NULL)
    {
        return NULL;
    }

    return project->database_path;
}
