/******************************************************************************
 * @file investigation_session.c
 * @brief Gestion d'une session d'enquête ouverte.
 ******************************************************************************/

#include "core/investigation_session.h"

#include "dao/investigation_dao.h"
#include "database/error.h"

/**
 * @brief Représentation privée d'une session d'enquête.
 */
struct InvestigationSession
{
    InvestigationProject *project;
    Database *database;
    InvestigationRecord *record;
};

/**
 * @brief Enregistre une erreur littérale si un GError est demandé.
 *
 * @param error Adresse recevant l'erreur.
 * @param error_code Code d'erreur du module.
 * @param error_message Message associé.
 */
static void investigation_session_set_error_literal(
    GError **error,
    InvestigationSessionError error_code,
    const char *error_message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        INVESTIGATION_SESSION_ERROR,
        error_code,
        error_message
    );
}

GQuark investigation_session_error_quark(void)
{
    return g_quark_from_static_string(
        "investigation-session-error-quark"
    );
}

InvestigationSession *investigation_session_open(
    const char *investigation_root_path,
    GError **error
)
{
    InvestigationSession *session = NULL;
    InvestigationProject *project = NULL;
    InvestigationRecord *record = NULL;
    Database *database = NULL;

    char *canonical_root_path = NULL;
    char *canonical_record_root_path = NULL;

    const char *database_path = NULL;
    const char *record_root_path = NULL;
    const char *database_error_message = NULL;

    /*
     * Convention GLib :
     *
     * un GError existant ne doit pas être remplacé.
     */
    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (investigation_root_path == NULL ||
        investigation_root_path[0] == '\0')
    {
        investigation_session_set_error_literal(
            error,
            INVESTIGATION_SESSION_ERROR_INVALID_ARGUMENT,
            "Le chemin racine de l'enquête est invalide."
        );

        return NULL;
    }

    canonical_root_path = g_canonicalize_filename(
        investigation_root_path,
        NULL
    );

    if (canonical_root_path == NULL)
    {
        investigation_session_set_error_literal(
            error,
            INVESTIGATION_SESSION_ERROR_MEMORY,
            "Impossible d'allouer le chemin canonique de l'enquête."
        );

        return NULL;
    }

    if (!g_file_test(
            canonical_root_path,
            G_FILE_TEST_IS_DIR
        ))
    {
        g_set_error(
            error,
            INVESTIGATION_SESSION_ERROR,
            INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND,
            "Le chemin racine '%s' n'est pas un dossier existant.",
            canonical_root_path
        );

        goto cleanup;
    }

    project = investigation_project_open(
        canonical_root_path
    );

    if (project == NULL)
    {
        investigation_session_set_error_literal(
            error,
            INVESTIGATION_SESSION_ERROR_PROJECT,
            "Impossible de construire le contexte du projet d'enquête."
        );

        goto cleanup;
    }

    database_path = investigation_project_get_database_path(
        project
    );

    if (database_path == NULL)
    {
        investigation_session_set_error_literal(
            error,
            INVESTIGATION_SESSION_ERROR_PROJECT,
            "Le projet ne fournit aucun chemin vers la base de données."
        );

        goto cleanup;
    }

    /*
     * Cette vérification doit précéder database_open().
     *
     * SQLite peut créer automatiquement une base absente lors de
     * l'ouverture. Une session ne doit jamais créer silencieusement
     * une nouvelle base.
     */
    if (!g_file_test(
            database_path,
            G_FILE_TEST_IS_REGULAR
        ))
    {
        g_set_error(
            error,
            INVESTIGATION_SESSION_ERROR,
            INVESTIGATION_SESSION_ERROR_DATABASE_NOT_FOUND,
            "Le fichier SQLite de l'enquête est absent : '%s'.",
            database_path
        );

        goto cleanup;
    }

    database = database_open(
        database_path
    );

    if (database == NULL)
    {
        g_set_error(
            error,
            INVESTIGATION_SESSION_ERROR,
            INVESTIGATION_SESSION_ERROR_DATABASE,
            "Impossible d'ouvrir la base de données '%s'.",
            database_path
        );

        goto cleanup;
    }

    if (!database_migrate_to_latest(
            database
        ))
    {
        database_error_message =
            database_error_get_message(
                database
            );

        if (database_error_message == NULL ||
            database_error_message[0] == '\0')
        {
            database_error_message =
                "La base de données n’a pas pu être mise à jour.";
        }

        g_set_error(
            error,
            INVESTIGATION_SESSION_ERROR,
            INVESTIGATION_SESSION_ERROR_DATABASE,
            "Impossible de mettre à jour le schéma SQLite : %s",
            database_error_message
        );

        goto cleanup;
    }

    record = investigation_dao_load(
        database
    );

    if (record == NULL)
    {
        /*
         * Le message doit être copié dans le GError avant la fermeture
         * de Database, car il appartient à l'objet Database.
         */
        database_error_message = database_error_get_message(
            database
        );

        if (database_error_message == NULL ||
            database_error_message[0] == '\0')
        {
            database_error_message =
                "Impossible de charger les informations de l'enquête.";
        }

        g_set_error(
            error,
            INVESTIGATION_SESSION_ERROR,
            INVESTIGATION_SESSION_ERROR_RECORD,
            "%s",
            database_error_message
        );

        goto cleanup;
    }

    record_root_path = investigation_record_get_root_path(
        record
    );

    if (record_root_path == NULL ||
        record_root_path[0] == '\0')
    {
        investigation_session_set_error_literal(
            error,
            INVESTIGATION_SESSION_ERROR_RECORD,
            "Le chemin racine enregistré dans la base est invalide."
        );

        goto cleanup;
    }

    canonical_record_root_path = g_canonicalize_filename(
        record_root_path,
        NULL
    );

    if (canonical_record_root_path == NULL)
    {
        investigation_session_set_error_literal(
            error,
            INVESTIGATION_SESSION_ERROR_MEMORY,
            "Impossible d'allouer le chemin canonique enregistré."
        );

        goto cleanup;
    }

    if (g_strcmp0(
            canonical_root_path,
            canonical_record_root_path
        ) != 0)
    {
        g_set_error(
            error,
            INVESTIGATION_SESSION_ERROR,
            INVESTIGATION_SESSION_ERROR_ROOT_MISMATCH,
            "Le chemin sélectionné '%s' ne correspond pas au chemin "
            "enregistré '%s'.",
            canonical_root_path,
            canonical_record_root_path
        );

        goto cleanup;
    }

    session = g_try_new0(
        InvestigationSession,
        1
    );

    if (session == NULL)
    {
        investigation_session_set_error_literal(
            error,
            INVESTIGATION_SESSION_ERROR_MEMORY,
            "Impossible d'allouer la session d'enquête."
        );

        goto cleanup;
    }

    /*
     * La session devient propriétaire des trois objets.
     */
    session->project = project;
    session->database = database;
    session->record = record;

    project = NULL;
    database = NULL;
    record = NULL;

cleanup:

    g_free(canonical_record_root_path);
    g_free(canonical_root_path);

    investigation_record_free(record);
    database_close(database);
    investigation_project_free(project);

    return session;
}

void investigation_session_close(
    InvestigationSession *session
)
{
    if (session == NULL)
    {
        return;
    }

    investigation_record_free(
        session->record
    );

    database_close(
        session->database
    );

    investigation_project_free(
        session->project
    );

    g_free(session);
}

const InvestigationProject *investigation_session_get_project(
    const InvestigationSession *session
)
{
    if (session == NULL)
    {
        return NULL;
    }

    return session->project;
}

const InvestigationRecord *investigation_session_get_record(
    const InvestigationSession *session
)
{
    if (session == NULL)
    {
        return NULL;
    }

    return session->record;
}

Database *investigation_session_get_database(
    InvestigationSession *session
)
{
    if (session == NULL)
    {
        return NULL;
    }

    return session->database;
}
