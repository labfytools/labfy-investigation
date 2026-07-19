/******************************************************************************
 * @file evidence_importer.c
 * @brief Orchestration de l’import transactionnel des preuves.
 ******************************************************************************/

#include <string.h>
#include <errno.h>
#include <glib/gstdio.h>

#include "core/evidence_importer.h"
#include "core/evidence_copy.h"
#include "dao/evidence_dao.h"
#include "database/transaction.h"
#include "database/error.h"

/**
 * @brief Longueur maximale conservée pour une extension.
 */
#define EVIDENCE_IMPORTER_MAX_EXTENSION_LENGTH 16U

#ifdef EVIDENCE_IMPORTER_ENABLE_TEST_HOOKS
#include "core/evidence_importer_test.h"
#endif

/**
 * @brief Représentation privée du service d’import.
 */
struct EvidenceImporter
{
    Database *database;
    EvidenceDao *evidence_dao;
};

#ifdef EVIDENCE_IMPORTER_ENABLE_TEST_HOOKS

static gboolean evidence_importer_test_cancel_after_copy = FALSE;
static gboolean evidence_importer_test_commit_failure = FALSE;
static gboolean evidence_importer_test_cleanup_failure = FALSE;

/**
 * @brief Consomme un hook à usage unique.
 */
static gboolean evidence_importer_test_take_hook(
    gboolean *hook
)
{
    gboolean enabled = FALSE;

    if (hook == NULL)
    {
        return FALSE;
    }

    enabled =
        *hook;

    *hook = FALSE;

    return enabled;
}

void evidence_importer_test_reset_hooks(void)
{
    evidence_importer_test_cancel_after_copy = FALSE;
    evidence_importer_test_commit_failure = FALSE;
    evidence_importer_test_cleanup_failure = FALSE;
}

void evidence_importer_test_set_cancel_after_copy(
    gboolean enabled
)
{
    evidence_importer_test_cancel_after_copy =
        enabled;
}

void evidence_importer_test_set_commit_failure(
    gboolean enabled
)
{
    evidence_importer_test_commit_failure =
        enabled;
}

void evidence_importer_test_set_cleanup_failure(
    gboolean enabled
)
{
    evidence_importer_test_cleanup_failure =
        enabled;
}

#endif

/**
 * @brief Informations générées avant la copie d’une preuve.
 */
typedef struct
{
    char *identifier;
    char *original_name;
    char *internal_name;
    char *relative_path;
} EvidenceImportStorage;

/**
 * @brief Libère les informations temporaires de stockage.
 */
static void evidence_importer_storage_clear(
    EvidenceImportStorage *storage
)
{
    if (storage == NULL)
    {
        return;
    }

    g_free(
        storage->relative_path
    );

    g_free(
        storage->internal_name
    );

    g_free(
        storage->original_name
    );

    g_free(
        storage->identifier
    );

    storage->relative_path = NULL;
    storage->internal_name = NULL;
    storage->original_name = NULL;
    storage->identifier = NULL;
}

/**
 * @brief Enregistre une erreur littérale EvidenceImporter.
 */
static void evidence_importer_set_error_literal(
    GError **error,
    EvidenceImporterError error_code,
    const char *error_message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_IMPORTER_ERROR,
        error_code,
        error_message
    );
}

/**
 * @brief Vérifie si une annulation a été demandée.
 */
static gboolean evidence_importer_check_cancelled(
    GCancellable *cancellable,
    GError **error
)
{
    if (cancellable == NULL ||
        !g_cancellable_is_cancelled(cancellable))
    {
        return FALSE;
    }

    evidence_importer_set_error_literal(
        error,
        EVIDENCE_IMPORTER_ERROR_CANCELLED,
        "L’import de la preuve a été annulé."
    );

    return TRUE;
}

/**
 * @brief Vérifie un chemin de dossier relatif à l’enquête.
 */
static gboolean evidence_importer_relative_directory_is_valid(
    const char *relative_directory
)
{
    const char *component_start = NULL;
    const char *cursor = NULL;

    gsize component_length = 0;

    if (relative_directory == NULL ||
        relative_directory[0] == '\0' ||
        g_path_is_absolute(relative_directory))
    {
        return FALSE;
    }

    component_start =
        relative_directory;

    for (cursor = relative_directory;
         ;
         cursor++)
    {
        /*
         * Les antislashs sont refusés pour éviter toute ambiguïté
         * entre systèmes.
         */
        if (*cursor == '\\')
        {
            return FALSE;
        }

        if (*cursor != '/' &&
            *cursor != '\0')
        {
            continue;
        }

        component_length =
            (gsize) (cursor - component_start);

        if (component_length == 0)
        {
            return FALSE;
        }

        if (component_length == 1 &&
            component_start[0] == '.')
        {
            return FALSE;
        }

        if (component_length == 2 &&
            component_start[0] == '.' &&
            component_start[1] == '.')
        {
            return FALSE;
        }

        if (*cursor == '\0')
        {
            break;
        }

        component_start =
            cursor + 1;
    }

    return TRUE;
}

/**
 * @brief Vérifie les champs obligatoires d’une requête.
 */
static gboolean evidence_importer_request_is_valid(
    const EvidenceImportRequest *request
)
{
    if (request == NULL)
    {
        return FALSE;
    }

    if (request->source_path == NULL ||
        request->source_path[0] == '\0')
    {
        return FALSE;
    }

    if (request->destination_directory == NULL ||
        request->destination_directory[0] == '\0')
    {
        return FALSE;
    }

    if (!evidence_importer_relative_directory_is_valid(
            request->relative_directory
        ))
    {
        return FALSE;
    }

    if (request->type_identifier == NULL ||
        request->type_identifier[0] == '\0')
    {
        return FALSE;
    }

    return TRUE;
}

GQuark evidence_importer_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-importer-error-quark"
    );
}

EvidenceImporter *evidence_importer_new(
    Database *database,
    GError **error
)
{
    EvidenceImporter *evidence_importer = NULL;
    EvidenceDao *evidence_dao = NULL;

    GError *dao_error = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
            "La connexion Database est invalide."
        );

        return NULL;
    }

    evidence_dao =
        evidence_dao_new(
            database,
            &dao_error
        );

    if (evidence_dao == NULL)
    {
        if (error != NULL)
        {
            g_set_error(
                error,
                EVIDENCE_IMPORTER_ERROR,
                EVIDENCE_IMPORTER_ERROR_DATABASE,
                "Impossible de créer le DAO des preuves : %s",
                dao_error != NULL
                    ? dao_error->message
                    : "erreur inconnue"
            );
        }

        g_clear_error(
            &dao_error
        );

        return NULL;
    }

    evidence_importer =
        g_try_new0(
            EvidenceImporter,
            1
        );

    if (evidence_importer == NULL)
    {
        evidence_dao_free(
            evidence_dao
        );

        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_MEMORY,
            "Impossible d’allouer le service d’import."
        );

        return NULL;
    }

    evidence_importer->database =
        database;

    evidence_importer->evidence_dao =
        evidence_dao;

    return evidence_importer;
}

/**
 * @brief Vérifie le nom extrait du chemin source.
 */
static gboolean evidence_importer_original_name_is_valid(
    const char *original_name
)
{
    if (original_name == NULL ||
        original_name[0] == '\0')
    {
        return FALSE;
    }

    if (strcmp(
            original_name,
            "."
        ) == 0 ||
        strcmp(
            original_name,
            ".."
        ) == 0)
    {
        return FALSE;
    }

    if (strchr(
            original_name,
            '/'
        ) != NULL ||
        strchr(
            original_name,
            '\\'
        ) != NULL)
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Génère le nom interne à partir de l’UUID et de l’extension.
 *
 * Une extension invalide est simplement ignorée.
 */
static gboolean evidence_importer_build_internal_name(
    const char *identifier,
    const char *original_name,
    char **out_internal_name,
    GError **error
)
{
    const char *extension = NULL;

    char *internal_name = NULL;

    gsize identifier_length = 0;
    gsize extension_length = 0;
    gsize allocation_size = 0;
    gsize extension_index = 0;

    gboolean extension_is_valid = FALSE;

    if (identifier == NULL ||
        original_name == NULL ||
        out_internal_name == NULL ||
        *out_internal_name != NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
            "Les paramètres du nom interne sont invalides."
        );

        return FALSE;
    }

    identifier_length =
        strlen(
            identifier
        );

    extension =
        strrchr(
            original_name,
            '.'
        );

    /*
     * Un fichier caché tel que ".profile" ne possède pas ici
     * d’extension exploitable.
     */
    if (extension != NULL &&
        extension != original_name &&
        extension[1] != '\0')
    {
        extension++;

        extension_length =
            strlen(
                extension
            );

        extension_is_valid =
            extension_length > 0 &&
            extension_length <=
                EVIDENCE_IMPORTER_MAX_EXTENSION_LENGTH;

        for (extension_index = 0;
             extension_is_valid &&
             extension_index < extension_length;
             extension_index++)
        {
            if (!g_ascii_isalnum(
                    extension[extension_index]
                ))
            {
                extension_is_valid = FALSE;
            }
        }
    }

    allocation_size =
        identifier_length + 1U;

    if (extension_is_valid)
    {
        allocation_size +=
            1U + extension_length;
    }

    internal_name =
        g_try_malloc(
            allocation_size
        );

    if (internal_name == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_MEMORY,
            "Impossible d’allouer le nom interne de la preuve."
        );

        return FALSE;
    }

    memcpy(
        internal_name,
        identifier,
        identifier_length
    );

    if (!extension_is_valid)
    {
        internal_name[identifier_length] = '\0';

        *out_internal_name =
            internal_name;

        return TRUE;
    }

    internal_name[identifier_length] = '.';

    for (extension_index = 0;
         extension_index < extension_length;
         extension_index++)
    {
        internal_name[
            identifier_length +
            1U +
            extension_index
        ] =
            (char) g_ascii_tolower(
                extension[extension_index]
            );
    }

    internal_name[
        identifier_length +
        1U +
        extension_length
    ] = '\0';

    *out_internal_name =
        internal_name;

    return TRUE;
}

/**
 * @brief Génère les informations nécessaires à la future copie.
 */
static gboolean evidence_importer_prepare_storage(
    const EvidenceImportRequest *request,
    EvidenceImportStorage *storage,
    GError **error
)
{
    if (request == NULL ||
        storage == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
            "Les paramètres de préparation du stockage sont invalides."
        );

        return FALSE;
    }

    storage->identifier =
        g_uuid_string_random();

    if (storage->identifier == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_MEMORY,
            "Impossible de générer l’identifiant de la preuve."
        );

        goto failure;
    }

    if (!g_uuid_string_is_valid(
            storage->identifier
        ))
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_DESTINATION,
            "L’identifiant généré pour la preuve est invalide."
        );

        goto failure;
    }

    storage->original_name =
        g_path_get_basename(
            request->source_path
        );

    if (!evidence_importer_original_name_is_valid(
            storage->original_name
        ))
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
            "Le nom original du fichier source est invalide."
        );

        goto failure;
    }

    if (!evidence_importer_build_internal_name(
            storage->identifier,
            storage->original_name,
            &storage->internal_name,
            error
        ))
    {
        goto failure;
    }

    storage->relative_path =
        g_build_filename(
            request->relative_directory,
            storage->internal_name,
            NULL
        );

    if (storage->relative_path == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_MEMORY,
            "Impossible de construire le chemin relatif de la preuve."
        );

        goto failure;
    }

    return TRUE;

failure:

    evidence_importer_storage_clear(
        storage
    );

    return FALSE;
}

void evidence_importer_free(
    EvidenceImporter *evidence_importer
)
{
    if (evidence_importer == NULL)
    {
        return;
    }

    evidence_dao_free(
        evidence_importer->evidence_dao
    );

    evidence_importer->evidence_dao = NULL;
    evidence_importer->database = NULL;

    g_free(
        evidence_importer
    );
}

/**
 * @brief Traduit une erreur EvidenceCopy en erreur EvidenceImporter.
 */
static EvidenceImporterError evidence_importer_map_copy_error(
    const GError *copy_error
)
{
    if (copy_error == NULL)
    {
        return EVIDENCE_IMPORTER_ERROR_COPY;
    }

    if (g_error_matches(
            copy_error,
            EVIDENCE_COPY_ERROR,
            EVIDENCE_COPY_ERROR_CANCELLED
        ))
    {
        return EVIDENCE_IMPORTER_ERROR_CANCELLED;
    }

    if (g_error_matches(
            copy_error,
            EVIDENCE_COPY_ERROR,
            EVIDENCE_COPY_ERROR_DESTINATION_INVALID
        ) ||
        g_error_matches(
            copy_error,
            EVIDENCE_COPY_ERROR,
            EVIDENCE_COPY_ERROR_DESTINATION_EXISTS
        ))
    {
        return EVIDENCE_IMPORTER_ERROR_DESTINATION;
    }

    if (g_error_matches(
            copy_error,
            EVIDENCE_COPY_ERROR,
            EVIDENCE_COPY_ERROR_CLEANUP
        ))
    {
        return EVIDENCE_IMPORTER_ERROR_ROLLBACK;
    }

    return EVIDENCE_IMPORTER_ERROR_COPY;
}

/**
 * @brief Conserve le contexte d’une erreur EvidenceCopy.
 */
static void evidence_importer_set_copy_error(
    GError **error,
    const GError *copy_error
)
{
    EvidenceImporterError error_code =
        evidence_importer_map_copy_error(
            copy_error
        );

    if (error == NULL)
    {
        return;
    }

    g_set_error(
        error,
        EVIDENCE_IMPORTER_ERROR,
        error_code,
        "Impossible de copier la preuve : %s",
        copy_error != NULL
            ? copy_error->message
            : "erreur de copie inconnue"
    );
}

/**
 * @brief Exécute le hook situé immédiatement après la copie.
 */
static void evidence_importer_after_copy(
    GCancellable *cancellable
)
{
#ifdef EVIDENCE_IMPORTER_ENABLE_TEST_HOOKS

    if (cancellable != NULL &&
        evidence_importer_test_take_hook(
            &evidence_importer_test_cancel_after_copy
        ))
    {
        g_cancellable_cancel(
            cancellable
        );
    }

#else

    (void) cancellable;

#endif
}

/**
 * @brief Valide la transaction, sauf panne simulée par un test.
 */
static gboolean evidence_importer_commit_transaction(
    Database *database
)
{
#ifdef EVIDENCE_IMPORTER_ENABLE_TEST_HOOKS

    if (evidence_importer_test_take_hook(
            &evidence_importer_test_commit_failure
        ))
    {
        database_error_set(
            database,
            DATABASE_ERROR_SQLITE,
            "Échec de COMMIT simulé par EvidenceImporter."
        );

        return FALSE;
    }

#endif

    return database_transaction_commit(
        database
    );
}

/**
 * @brief Supprime un chemin, sauf panne simulée par un test.
 */
static int evidence_importer_remove_path(
    const char *path
)
{
#ifdef EVIDENCE_IMPORTER_ENABLE_TEST_HOOKS

    if (evidence_importer_test_take_hook(
            &evidence_importer_test_cleanup_failure
        ))
    {
        errno = EACCES;

        return -1;
    }

#endif

    return g_remove(
        path
    );
}

/**
 * @brief Supprime une copie qui ne peut pas être conservée.
 */
static gboolean evidence_importer_remove_copy(
    const EvidenceCopyResult *copy_result,
    GError **error
)
{
    const char *destination_path = NULL;

    char *primary_error_message = NULL;

    int saved_errno = 0;

    if (copy_result == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
            "Le résultat de copie à supprimer est invalide."
        );

        return FALSE;
    }

    destination_path =
        evidence_copy_result_get_destination_path(
            copy_result
        );

    if (destination_path == NULL ||
        destination_path[0] == '\0')
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_ROLLBACK,
            "Le chemin de la copie à supprimer est invalide."
        );

        return FALSE;
    }

    if (evidence_importer_remove_path(
            destination_path
        ) == 0)
    {
        return TRUE;
    }

    saved_errno = errno;

    if (error == NULL)
    {
        g_warning(
            "Impossible de supprimer la copie '%s' : %s",
            destination_path,
            g_strerror(saved_errno)
        );

        return FALSE;
    }

    if (*error != NULL)
    {
        primary_error_message =
            g_strdup(
                (*error)->message
            );

        g_clear_error(
            error
        );
    }

    g_set_error(
        error,
        EVIDENCE_IMPORTER_ERROR,
        EVIDENCE_IMPORTER_ERROR_ROLLBACK,
        "La copie '%s' n’a pas pu être supprimée : %s. "
        "Cause initiale : %s",
        destination_path,
        g_strerror(saved_errno),
        primary_error_message != NULL
            ? primary_error_message
            : "erreur inconnue"
    );

    g_free(
        primary_error_message
    );

    return FALSE;
}

/**
 * @brief Génère la date UTC actuelle au format EvidenceRecord.
 */
static char *evidence_importer_create_imported_at(
    GError **error
)
{
    GDateTime *current_time = NULL;

    char *imported_at = NULL;

    current_time =
        g_date_time_new_now_utc();

    if (current_time == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_MEMORY,
            "Impossible de créer la date UTC d’importation."
        );

        return NULL;
    }

    imported_at =
        g_date_time_format(
            current_time,
            "%Y-%m-%dT%H:%M:%SZ"
        );

    g_date_time_unref(
        current_time
    );

    if (imported_at == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_MEMORY,
            "Impossible de formater la date UTC d’importation."
        );

        return NULL;
    }

    return imported_at;
}

/**
 * @brief Conserve le contexte d’une erreur EvidenceRecord.
 */
static void evidence_importer_set_model_error(
    GError **error,
    const GError *model_error
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error(
        error,
        EVIDENCE_IMPORTER_ERROR,
        EVIDENCE_IMPORTER_ERROR_MODEL,
        "Impossible de créer le modèle de preuve : %s",
        model_error != NULL
            ? model_error->message
            : "erreur de modèle inconnue"
    );
}

/**
 * @brief Conserve le contexte d’une erreur EvidenceDao.
 */
static void evidence_importer_set_dao_error(
    GError **error,
    const GError *dao_error
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error(
        error,
        EVIDENCE_IMPORTER_ERROR,
        EVIDENCE_IMPORTER_ERROR_DATABASE,
        "Impossible d’enregistrer la preuve : %s",
        dao_error != NULL
            ? dao_error->message
            : "erreur DAO inconnue"
    );
}

/**
 * @brief Conserve le contexte d’une erreur Database.
 */
static void evidence_importer_set_database_error(
    GError **error,
    const char *context,
    const Database *database
)
{
    const char *database_message = NULL;

    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    database_message =
        database_error_get_message(
            database
        );

    g_set_error(
        error,
        EVIDENCE_IMPORTER_ERROR,
        EVIDENCE_IMPORTER_ERROR_DATABASE,
        "%s : %s",
        context,
        database_message != NULL
            ? database_message
            : "erreur Database inconnue"
    );
}

/**
 * @brief Annule la transaction démarrée par l’importeur.
 */
static gboolean evidence_importer_rollback_transaction(
    Database *database,
    GError **error
)
{
    const char *database_message = NULL;

    char *primary_error_message = NULL;

    if (database == NULL)
    {
        return FALSE;
    }

    if (!database_transaction_is_active(
            database
        ))
    {
        return TRUE;
    }

    if (database_transaction_rollback(
            database
        ))
    {
        return TRUE;
    }

    database_message =
        database_error_get_message(
            database
        );

    if (error == NULL)
    {
        g_warning(
            "Impossible d’annuler la transaction SQLite : %s",
            database_message != NULL
                ? database_message
                : "erreur inconnue"
        );

        return FALSE;
    }

    if (*error != NULL)
    {
        primary_error_message =
            g_strdup(
                (*error)->message
            );

        g_clear_error(
            error
        );
    }

    g_set_error(
        error,
        EVIDENCE_IMPORTER_ERROR,
        EVIDENCE_IMPORTER_ERROR_ROLLBACK,
        "Impossible d’annuler la transaction SQLite : %s. "
        "Cause initiale : %s",
        database_message != NULL
            ? database_message
            : "erreur inconnue",
        primary_error_message != NULL
            ? primary_error_message
            : "erreur inconnue"
    );

    g_free(
        primary_error_message
    );

    return FALSE;
}

EvidenceRecord *evidence_importer_import(
    EvidenceImporter *evidence_importer,
    const EvidenceImportRequest *request,
    GCancellable *cancellable,
    GError **error
)
{
    EvidenceImportStorage storage = {0};

    EvidenceCopyResult *copy_result = NULL;
    EvidenceRecord *evidence_record = NULL;

    GError *copy_error = NULL;
    GError *model_error = NULL;
    GError *dao_error = NULL;

    char *imported_at = NULL;

    gboolean transaction_started = FALSE;
    gboolean success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (evidence_importer == NULL ||
        evidence_importer->database == NULL ||
        evidence_importer->evidence_dao == NULL)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
            "Le service d’import est invalide."
        );

        return NULL;
    }

    if (!evidence_importer_request_is_valid(
            request
        ))
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
            "La requête d’import est invalide."
        );

        return NULL;
    }

    if (evidence_importer_check_cancelled(
            cancellable,
            error
        ))
    {
        return NULL;
    }

    /*
     * Le refus doit survenir avant toute copie.
     */
    if (database_transaction_is_active(
            evidence_importer->database
        ))
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_DATABASE,
            "Une transaction Database est déjà active."
        );

        return NULL;
    }

    if (!evidence_importer_prepare_storage(
            request,
            &storage,
            error
        ))
    {
        goto cleanup;
    }

    copy_result =
        evidence_copy_file(
            request->source_path,
            request->destination_directory,
            storage.internal_name,
            cancellable,
            &copy_error
        );

    if (copy_result == NULL)
    {
        evidence_importer_set_copy_error(
            error,
            copy_error
        );

        goto cleanup;
    }

    evidence_importer_after_copy(
        cancellable
    );

    if (g_strcmp0(
            evidence_copy_result_get_destination_name(
                copy_result
            ),
            storage.internal_name
        ) != 0)
    {
        evidence_importer_set_error_literal(
            error,
            EVIDENCE_IMPORTER_ERROR_COPY,
            "Le nom retourné par EvidenceCopy ne correspond pas "
            "au nom interne généré."
        );

        goto cleanup;
    }

    if (evidence_importer_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    imported_at =
        evidence_importer_create_imported_at(
            error
        );

    if (imported_at == NULL)
    {
        goto cleanup;
    }

    evidence_record =
        evidence_record_new(
            storage.identifier,
            storage.original_name,
            storage.internal_name,
            storage.relative_path,
            request->type_identifier,
            evidence_copy_result_get_size_bytes(
                copy_result
            ),
            evidence_copy_result_get_sha256(
                copy_result
            ),
            imported_at,
            request->collected_at,
            request->source,
            request->description,
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &model_error
        );

    if (evidence_record == NULL)
    {
        evidence_importer_set_model_error(
            error,
            model_error
        );

        goto cleanup;
    }

    if (evidence_importer_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    if (!database_transaction_begin(
            evidence_importer->database
        ))
    {
        evidence_importer_set_database_error(
            error,
            "Impossible de démarrer la transaction SQLite",
            evidence_importer->database
        );

        goto cleanup;
    }

    transaction_started = TRUE;

    if (evidence_importer_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    if (!evidence_dao_insert(
            evidence_importer->evidence_dao,
            evidence_record,
            &dao_error
        ))
    {
        evidence_importer_set_dao_error(
            error,
            dao_error
        );

        goto cleanup;
    }

    if (evidence_importer_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    if (!evidence_importer_commit_transaction(
            evidence_importer->database
        ))
    {
        evidence_importer_set_database_error(
            error,
            "Impossible de valider la transaction SQLite",
            evidence_importer->database
        );

        goto cleanup;
    }

    transaction_started = FALSE;
    success = TRUE;

cleanup:

    g_clear_error(
        &dao_error
    );

    g_clear_error(
        &model_error
    );

    g_clear_error(
        &copy_error
    );

    if (!success &&
        transaction_started)
    {
        evidence_importer_rollback_transaction(
            evidence_importer->database,
            error
        );

        transaction_started = FALSE;
    }

    /*
     * Une copie n’est conservée que si le commit SQLite a réussi.
     */
    if (!success &&
        copy_result != NULL)
    {
        evidence_importer_remove_copy(
            copy_result,
            error
        );
    }

    evidence_copy_result_free(
        copy_result
    );

    g_free(
        imported_at
    );

    evidence_importer_storage_clear(
        &storage
    );

    if (!success)
    {
        evidence_record_free(
            evidence_record
        );

        return NULL;
    }

    /*
     * Le fichier et la ligne SQLite sont maintenant validés.
     * Le modèle appartient à l’appelant.
     */
    return evidence_record;
}
