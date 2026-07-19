/******************************************************************************
 * @file evidence_copy.c
 * @brief Copie sûre et vérifiée des fichiers de preuve.
 ******************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include "core/evidence_copy.h"
#include "core/file_hash.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Taille du tampon utilisé pour la copie.
 */
#define EVIDENCE_COPY_BLOCK_SIZE (64U * 1024U)

#ifdef EVIDENCE_COPY_ENABLE_TEST_HOOKS

static EvidenceCopyTestBlockHook
    evidence_copy_test_block_hook = NULL;

static gpointer
    evidence_copy_test_block_hook_data = NULL;

void evidence_copy_test_set_block_hook(
    EvidenceCopyTestBlockHook block_hook,
    gpointer user_data
)
{
    evidence_copy_test_block_hook =
        block_hook;

    evidence_copy_test_block_hook_data =
        user_data;
}

static void evidence_copy_test_notify_block(
    guint64 copied_size_bytes
)
{
    if (evidence_copy_test_block_hook == NULL)
    {
        return;
    }

    evidence_copy_test_block_hook(
        copied_size_bytes,
        evidence_copy_test_block_hook_data
    );
}

static EvidenceCopyTestBeforeWriteHook
    evidence_copy_test_before_write_hook = NULL;

static gpointer
    evidence_copy_test_before_write_hook_data = NULL;

void evidence_copy_test_set_before_write_hook(
    EvidenceCopyTestBeforeWriteHook before_write_hook,
    gpointer user_data
)
{
    evidence_copy_test_before_write_hook =
        before_write_hook;

    evidence_copy_test_before_write_hook_data =
        user_data;
}

static gboolean evidence_copy_test_should_fail_write(
    guint64 written_size_bytes
)
{
    if (evidence_copy_test_before_write_hook == NULL)
    {
        return FALSE;
    }

    return evidence_copy_test_before_write_hook(
        written_size_bytes,
        evidence_copy_test_before_write_hook_data
    );
}

/**
 * @brief Crochet appelé avant la vérification cryptographique finale.
 */
typedef void (*EvidenceCopyTestBeforeVerifyHook)(
    const char *destination_path,
    gpointer user_data
);

/**
 * @brief Configure le crochet de vérification réservé aux tests.
 */
void evidence_copy_test_set_before_verify_hook(
    EvidenceCopyTestBeforeVerifyHook before_verify_hook,
    gpointer user_data
);

static EvidenceCopyTestBeforeVerifyHook
    evidence_copy_test_before_verify_hook = NULL;

static gpointer
    evidence_copy_test_before_verify_hook_data = NULL;

void evidence_copy_test_set_before_verify_hook(
    EvidenceCopyTestBeforeVerifyHook before_verify_hook,
    gpointer user_data
)
{
    evidence_copy_test_before_verify_hook =
        before_verify_hook;

    evidence_copy_test_before_verify_hook_data =
        user_data;
}

static void evidence_copy_test_notify_before_verify(
    const char *destination_path
)
{
    if (evidence_copy_test_before_verify_hook == NULL)
    {
        return;
    }

    evidence_copy_test_before_verify_hook(
        destination_path,
        evidence_copy_test_before_verify_hook_data
    );
}

static EvidenceCopyTestCleanupHook
    evidence_copy_test_cleanup_hook = NULL;

static gpointer
    evidence_copy_test_cleanup_hook_data = NULL;

void evidence_copy_test_set_cleanup_hook(
    EvidenceCopyTestCleanupHook cleanup_hook,
    gpointer user_data
)
{
    evidence_copy_test_cleanup_hook =
        cleanup_hook;

    evidence_copy_test_cleanup_hook_data =
        user_data;
}

static gboolean evidence_copy_test_should_fail_cleanup(
    const char *destination_path
)
{
    if (evidence_copy_test_cleanup_hook == NULL)
    {
        return FALSE;
    }

    return evidence_copy_test_cleanup_hook(
        destination_path,
        evidence_copy_test_cleanup_hook_data
    );
}

#else

static void evidence_copy_test_notify_block(
    guint64 copied_size_bytes
)
{
    (void) copied_size_bytes;
}

static gboolean evidence_copy_test_should_fail_write(
    guint64 written_size_bytes
)
{
    (void) written_size_bytes;

    return FALSE;
}

static void evidence_copy_test_notify_before_verify(
    const char *destination_path
)
{
    (void) destination_path;
}

static gboolean evidence_copy_test_should_fail_cleanup(
    const char *destination_path
)
{
    (void) destination_path;

    return FALSE;
}

#endif

/**
 * @brief Représentation privée du résultat d’une copie validée.
 */
struct EvidenceCopyResult
{
    char *destination_path;
    char *destination_name;
    char *sha256;

    guint64 size_bytes;
};

/**
 * @brief Enregistre une erreur littérale EvidenceCopy.
 */
static void evidence_copy_set_error_literal(
    GError **error,
    EvidenceCopyError error_code,
    const char *error_message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_COPY_ERROR,
        error_code,
        error_message
    );
}

/**
 * @brief Enregistre une erreur système EvidenceCopy.
 */
static void evidence_copy_set_system_error(
    GError **error,
    EvidenceCopyError error_code,
    const char *context,
    const char *file_path,
    int system_error
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error(
        error,
        EVIDENCE_COPY_ERROR,
        error_code,
        "%s '%s' : %s",
        context,
        file_path,
        g_strerror(system_error)
    );
}

/**
 * @brief Vérifie si une annulation a été demandée.
 */
static gboolean evidence_copy_check_cancelled(
    GCancellable *cancellable,
    GError **error
)
{
    if (cancellable == NULL ||
        !g_cancellable_is_cancelled(cancellable))
    {
        return FALSE;
    }

    evidence_copy_set_error_literal(
        error,
        EVIDENCE_COPY_ERROR_CANCELLED,
        "La copie de la preuve a été annulée."
    );

    return TRUE;
}

/**
 * @brief Vérifie qu’un nom représente uniquement un fichier.
 */
static gboolean evidence_copy_destination_name_is_valid(
    const char *destination_name
)
{
    if (destination_name == NULL ||
        destination_name[0] == '\0')
    {
        return FALSE;
    }

    if (strcmp(
            destination_name,
            "."
        ) == 0 ||
        strcmp(
            destination_name,
            ".."
        ) == 0)
    {
        return FALSE;
    }

    if (strchr(
            destination_name,
            '/'
        ) != NULL ||
        strchr(
            destination_name,
            '\\'
        ) != NULL)
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Traduit une erreur FileHash en erreur EvidenceCopy.
 */
static EvidenceCopyError evidence_copy_map_hash_error(
    const GError *hash_error
)
{
    if (hash_error == NULL)
    {
        return EVIDENCE_COPY_ERROR_HASH;
    }

    if (g_error_matches(
            hash_error,
            FILE_HASH_ERROR,
            FILE_HASH_ERROR_NOT_FOUND
        ))
    {
        return EVIDENCE_COPY_ERROR_SOURCE_NOT_FOUND;
    }

    if (g_error_matches(
            hash_error,
            FILE_HASH_ERROR,
            FILE_HASH_ERROR_NOT_REGULAR
        ))
    {
        return EVIDENCE_COPY_ERROR_SOURCE_NOT_REGULAR;
    }

    if (g_error_matches(
            hash_error,
            FILE_HASH_ERROR,
            FILE_HASH_ERROR_CANCELLED
        ))
    {
        return EVIDENCE_COPY_ERROR_CANCELLED;
    }

    return EVIDENCE_COPY_ERROR_HASH;
}

/**
 * @brief Convertit et conserve le contexte d’une erreur FileHash.
 */
static void evidence_copy_set_hash_error(
    GError **error,
    const GError *hash_error
)
{
    EvidenceCopyError error_code =
        evidence_copy_map_hash_error(
            hash_error
        );

    if (error == NULL)
    {
        return;
    }

    g_set_error(
        error,
        EVIDENCE_COPY_ERROR,
        error_code,
        "Impossible d’analyser le fichier source : %s",
        hash_error != NULL
            ? hash_error->message
            : "erreur SHA-256 inconnue"
    );
}

GQuark evidence_copy_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-copy-error-quark"
    );
}

/**
 * @brief Ouvre et valide le dossier de destination.
 *
 * Le dernier composant du chemin ne peut pas être un lien symbolique.
 *
 * @return Descripteur du dossier, ou -1.
 */
static int evidence_copy_open_destination_directory(
    const char *destination_directory,
    GError **error
)
{
    struct stat directory_status;

    int directory_descriptor = -1;
    int saved_errno = 0;

    directory_descriptor = open(
        destination_directory,
        O_RDONLY |
        O_DIRECTORY |
        O_CLOEXEC |
        O_NOFOLLOW
    );

    if (directory_descriptor < 0)
    {
        saved_errno = errno;

        evidence_copy_set_system_error(
            error,
            EVIDENCE_COPY_ERROR_DESTINATION_INVALID,
            "Impossible d’ouvrir le dossier de destination",
            destination_directory,
            saved_errno
        );

        return -1;
    }

    if (fstat(
            directory_descriptor,
            &directory_status
        ) != 0)
    {
        saved_errno = errno;

        evidence_copy_set_system_error(
            error,
            EVIDENCE_COPY_ERROR_DESTINATION_INVALID,
            "Impossible d’inspecter le dossier de destination",
            destination_directory,
            saved_errno
        );

        close(
            directory_descriptor
        );

        return -1;
    }

    if (!S_ISDIR(directory_status.st_mode))
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_DESTINATION_INVALID,
            "Le chemin de destination ne désigne pas un dossier."
        );

        close(
            directory_descriptor
        );

        return -1;
    }

    return directory_descriptor;
}

/**
 * @brief Ouvre et valide le fichier source.
 *
 * @return Descripteur du fichier source, ou -1.
 */
static int evidence_copy_open_source_file(
    const char *source_path,
    GError **error
)
{
    struct stat source_status;

    int source_descriptor = -1;
    int saved_errno = 0;

    source_descriptor = open(
        source_path,
        O_RDONLY |
        O_CLOEXEC |
        O_NOFOLLOW |
        O_NONBLOCK
    );

    if (source_descriptor < 0)
    {
        saved_errno = errno;

        if (saved_errno == ENOENT ||
            saved_errno == ENOTDIR)
        {
            evidence_copy_set_system_error(
                error,
                EVIDENCE_COPY_ERROR_SOURCE_NOT_FOUND,
                "Le fichier source est introuvable",
                source_path,
                saved_errno
            );
        }
        else if (saved_errno == ELOOP)
        {
            evidence_copy_set_error_literal(
                error,
                EVIDENCE_COPY_ERROR_SOURCE_NOT_REGULAR,
                "Le fichier source est un lien symbolique."
            );
        }
        else
        {
            evidence_copy_set_system_error(
                error,
                EVIDENCE_COPY_ERROR_OPEN_SOURCE,
                "Impossible d’ouvrir le fichier source",
                source_path,
                saved_errno
            );
        }

        return -1;
    }

    if (fstat(
            source_descriptor,
            &source_status
        ) != 0)
    {
        saved_errno = errno;

        evidence_copy_set_system_error(
            error,
            EVIDENCE_COPY_ERROR_OPEN_SOURCE,
            "Impossible d’inspecter le fichier source",
            source_path,
            saved_errno
        );

        close(
            source_descriptor
        );

        return -1;
    }

    if (!S_ISREG(source_status.st_mode))
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_SOURCE_NOT_REGULAR,
            "Le chemin source ne désigne pas un fichier régulier."
        );

        close(
            source_descriptor
        );

        return -1;
    }

    return source_descriptor;
}

/**
 * @brief Écrit intégralement un bloc dans le fichier destination.
 */
static gboolean evidence_copy_write_all(
    int destination_descriptor,
    const guint8 *buffer,
    gsize buffer_size,
    guint64 copied_size_before_block,
    const char *destination_path,
    GCancellable *cancellable,
    GError **error
)
{
    gsize written_size = 0;

    ssize_t current_write_size = 0;

    int saved_errno = 0;

    while (written_size < buffer_size)
    {
        if (evidence_copy_check_cancelled(
                cancellable,
                error
            ))
        {
            return FALSE;
        }

        if (evidence_copy_test_should_fail_write(
                copied_size_before_block +
                (guint64) written_size
            ))
        {
            evidence_copy_set_error_literal(
                error,
                EVIDENCE_COPY_ERROR_WRITE,
                "Une erreur d’écriture simulée est survenue."
            );

            return FALSE;
        }

        current_write_size = write(
            destination_descriptor,
            buffer + written_size,
            buffer_size - written_size
        );

        if (current_write_size < 0)
        {
            saved_errno = errno;

            if (saved_errno == EINTR)
            {
                continue;
            }

            evidence_copy_set_system_error(
                error,
                EVIDENCE_COPY_ERROR_WRITE,
                "Impossible d’écrire dans le fichier destination",
                destination_path,
                saved_errno
            );

            return FALSE;
        }

        if (current_write_size == 0)
        {
            evidence_copy_set_error_literal(
                error,
                EVIDENCE_COPY_ERROR_WRITE,
                "L’écriture du fichier destination n’a écrit aucun octet."
            );

            return FALSE;
        }

        written_size +=
            (gsize) current_write_size;
    }

    return TRUE;
}

/**
 * @brief Copie progressivement le contenu du fichier source.
 */
static gboolean evidence_copy_contents(
    int source_descriptor,
    int destination_descriptor,
    const char *source_path,
    const char *destination_path,
    GCancellable *cancellable,
    guint64 *out_copied_size_bytes,
    GError **error
)
{
    guint8 *buffer = NULL;

    guint64 copied_size_bytes = 0;

    ssize_t read_size = 0;

    int saved_errno = 0;

    if (out_copied_size_bytes == NULL)
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_INVALID_ARGUMENT,
            "La destination de la taille copiée est invalide."
        );

        return FALSE;
    }

    *out_copied_size_bytes = 0;

    buffer = g_try_malloc(
        EVIDENCE_COPY_BLOCK_SIZE
    );

    if (buffer == NULL)
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_MEMORY,
            "Impossible d’allouer le tampon de copie."
        );

        return FALSE;
    }

    while (TRUE)
    {
        if (evidence_copy_check_cancelled(
                cancellable,
                error
            ))
        {
            g_free(
                buffer
            );

            return FALSE;
        }

        read_size = read(
            source_descriptor,
            buffer,
            EVIDENCE_COPY_BLOCK_SIZE
        );

        if (read_size < 0)
        {
            saved_errno = errno;

            if (saved_errno == EINTR)
            {
                continue;
            }

            evidence_copy_set_system_error(
                error,
                EVIDENCE_COPY_ERROR_READ,
                "Impossible de lire le fichier source",
                source_path,
                saved_errno
            );

            g_free(
                buffer
            );

            return FALSE;
        }

        if (read_size == 0)
        {
            break;
        }

        if (!evidence_copy_write_all(
                destination_descriptor,
                buffer,
                (gsize) read_size,
                copied_size_bytes,
                destination_path,
                cancellable,
                error
            ))
        {
            g_free(
                buffer
            );

            return FALSE;
        }

        copied_size_bytes +=
            (guint64) read_size;

        evidence_copy_test_notify_block(
            copied_size_bytes
        );

        if (evidence_copy_check_cancelled(
                cancellable,
                error
            ))
        {
            g_free(
                buffer
            );

            return FALSE;
        }
    }

    g_free(
        buffer
    );

    *out_copied_size_bytes =
        copied_size_bytes;

    return TRUE;
}

/**
 * @brief Crée exclusivement le fichier destination.
 *
 * Le fichier ne peut pas remplacer un élément existant.
 *
 * @return Descripteur du fichier créé, ou -1.
 */
static int evidence_copy_create_destination_file(
    int destination_directory_descriptor,
    const char *destination_name,
    const char *destination_path,
    GError **error
)
{
    int destination_descriptor = -1;
    int saved_errno = 0;

    destination_descriptor = openat(
        destination_directory_descriptor,
        destination_name,
        O_WRONLY |
        O_CREAT |
        O_EXCL |
        O_CLOEXEC |
        O_NOFOLLOW,
        0600
    );

    if (destination_descriptor >= 0)
    {
        return destination_descriptor;
    }

    saved_errno = errno;

    if (saved_errno == EEXIST)
    {
        evidence_copy_set_system_error(
            error,
            EVIDENCE_COPY_ERROR_DESTINATION_EXISTS,
            "Le fichier destination existe déjà",
            destination_path,
            saved_errno
        );
    }
    else
    {
        evidence_copy_set_system_error(
            error,
            EVIDENCE_COPY_ERROR_CREATE_DESTINATION,
            "Impossible de créer le fichier destination",
            destination_path,
            saved_errno
        );
    }

    return -1;
}

/**
 * @brief Duplique une chaîne en signalant proprement une erreur mémoire.
 */
static char *evidence_copy_duplicate_string(
    const char *value,
    GError **error
)
{
    char *copy = NULL;
    gsize value_size = 0;

    if (value == NULL)
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_INVALID_ARGUMENT,
            "La chaîne à dupliquer est invalide."
        );

        return NULL;
    }

    value_size =
        strlen(
            value
        );

    copy = g_try_malloc(
        value_size + 1U
    );

    if (copy == NULL)
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_MEMORY,
            "Impossible d’allouer une chaîne du résultat de copie."
        );

        return NULL;
    }

    memcpy(
        copy,
        value,
        value_size + 1U
    );

    return copy;
}

/**
 * @brief Signale qu’un fichier incomplet n’a pas pu être supprimé.
 */
static void evidence_copy_report_cleanup_failure(
    GError **error,
    const char *destination_path,
    int system_error
)
{
    char *primary_message = NULL;

    if (error == NULL)
    {
        g_warning(
            "Impossible de supprimer la copie incomplète '%s' : %s",
            destination_path,
            g_strerror(system_error)
        );

        return;
    }

    if (*error != NULL)
    {
        primary_message =
            g_strdup(
                (*error)->message
            );

        g_clear_error(
            error
        );
    }

    g_set_error(
        error,
        EVIDENCE_COPY_ERROR,
        EVIDENCE_COPY_ERROR_CLEANUP,
        "Un fichier incomplet peut subsister à l’emplacement '%s' : %s. "
        "Cause initiale : %s",
        destination_path,
        g_strerror(system_error),
        primary_message != NULL
            ? primary_message
            : "erreur inconnue"
    );

    g_free(
        primary_message
    );
}

/**
 * @brief Supprime une destination invalide.
 *
 * @return 0 en cas de succès, sinon -1 avec errno renseigné.
 */
static int evidence_copy_remove_invalid_destination(
    int destination_directory_descriptor,
    const char *destination_name,
    const char *destination_path
)
{
    if (evidence_copy_test_should_fail_cleanup(
            destination_path
        ))
    {
        errno = EACCES;

        return -1;
    }

    return unlinkat(
        destination_directory_descriptor,
        destination_name,
        0
    );
}

EvidenceCopyResult *evidence_copy_file(
    const char *source_path,
    const char *destination_directory,
    const char *destination_name,
    GCancellable *cancellable,
    GError **error
)
{
    EvidenceCopyResult *result = NULL;

    GError *hash_error = NULL;

    char *source_sha256_before = NULL;
    char *source_sha256_after = NULL;
    char *destination_sha256 = NULL;

    char *destination_path = NULL;
    char *destination_name_copy = NULL;

    guint64 source_size_before = 0;
    guint64 source_size_after = 0;
    guint64 destination_size = 0;
    guint64 copied_size_bytes = 0;

    int source_descriptor = -1;
    int destination_directory_descriptor = -1;
    int destination_descriptor = -1;
    int cleanup_errno = 0;

    gboolean destination_created = FALSE;
    gboolean success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (source_path == NULL ||
        source_path[0] == '\0')
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_INVALID_ARGUMENT,
            "Le chemin du fichier source est invalide."
        );

        return NULL;
    }

    if (destination_directory == NULL ||
        destination_directory[0] == '\0')
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_INVALID_ARGUMENT,
            "Le dossier de destination est invalide."
        );

        return NULL;
    }

    if (!evidence_copy_destination_name_is_valid(
            destination_name
        ))
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_INVALID_ARGUMENT,
            "Le nom du fichier destination est invalide."
        );

        return NULL;
    }

    if (evidence_copy_check_cancelled(
            cancellable,
            error
        ))
    {
        return NULL;
    }

    /*
     * Première photographie cryptographique du fichier source.
     */
    if (!file_hash_compute_sha256(
            source_path,
            cancellable,
            &source_sha256_before,
            &source_size_before,
            &hash_error
        ))
    {
        evidence_copy_set_hash_error(
            error,
            hash_error
        );

        g_clear_error(
            &hash_error
        );

        return NULL;
    }

    destination_path =
        g_build_filename(
            destination_directory,
            destination_name,
            NULL
        );

    if (destination_path == NULL)
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_MEMORY,
            "Impossible de construire le chemin de destination."
        );

        goto cleanup;
    }

    source_descriptor =
        evidence_copy_open_source_file(
            source_path,
            error
        );

    if (source_descriptor < 0)
    {
        goto cleanup;
    }

    destination_directory_descriptor =
        evidence_copy_open_destination_directory(
            destination_directory,
            error
        );

    if (destination_directory_descriptor < 0)
    {
        goto cleanup;
    }

    if (evidence_copy_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    destination_descriptor =
        evidence_copy_create_destination_file(
            destination_directory_descriptor,
            destination_name,
            destination_path,
            error
        );

    if (destination_descriptor < 0)
    {
        goto cleanup;
    }

    destination_created = TRUE;

    if (!evidence_copy_contents(
            source_descriptor,
            destination_descriptor,
            source_path,
            destination_path,
            cancellable,
            &copied_size_bytes,
            error
        ))
    {
        goto cleanup;
    }

    if (evidence_copy_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    if (fsync(
            destination_descriptor
        ) != 0)
    {
        evidence_copy_set_system_error(
            error,
            EVIDENCE_COPY_ERROR_SYNC,
            "Impossible de synchroniser le fichier destination",
            destination_path,
            errno
        );

        goto cleanup;
    }

    /*
     * Un échec de fermeture d’un fichier écrit est considéré comme
     * un échec de finalisation.
     */
    if (close(
            destination_descriptor
        ) != 0)
    {
        destination_descriptor = -1;

        evidence_copy_set_system_error(
            error,
            EVIDENCE_COPY_ERROR_SYNC,
            "Impossible de fermer le fichier destination",
            destination_path,
            errno
        );

        goto cleanup;
    }

    destination_descriptor = -1;

    if (close(
            source_descriptor
        ) != 0)
    {
        g_warning(
            "Impossible de fermer proprement le fichier source '%s' : %s",
            source_path,
            g_strerror(errno)
        );
    }

    source_descriptor = -1;

    if (evidence_copy_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    evidence_copy_test_notify_before_verify(
        destination_path
    );

    /*
     * Vérification cryptographique de la copie.
     */
    if (!file_hash_compute_sha256(
            destination_path,
            cancellable,
            &destination_sha256,
            &destination_size,
            &hash_error
        ))
    {
        if (error != NULL)
        {
            g_set_error(
                error,
                EVIDENCE_COPY_ERROR,
                EVIDENCE_COPY_ERROR_HASH,
                "Impossible de vérifier le fichier destination : %s",
                hash_error != NULL
                    ? hash_error->message
                    : "erreur SHA-256 inconnue"
            );
        }

        g_clear_error(
            &hash_error
        );

        goto cleanup;
    }

    if (evidence_copy_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    /*
     * Deuxième photographie du source afin de détecter une modification
     * survenue pendant la copie.
     */
    if (!file_hash_compute_sha256(
            source_path,
            cancellable,
            &source_sha256_after,
            &source_size_after,
            &hash_error
        ))
    {
        evidence_copy_set_hash_error(
            error,
            hash_error
        );

        g_clear_error(
            &hash_error
        );

        goto cleanup;
    }

    if (copied_size_bytes != source_size_before ||
        destination_size != source_size_before ||
        source_size_after != source_size_before ||
        strcmp(
            destination_sha256,
            source_sha256_before
        ) != 0 ||
        strcmp(
            source_sha256_after,
            source_sha256_before
        ) != 0)
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_VERIFY,
            "La source ou sa copie a changé pendant l’opération."
        );

        goto cleanup;
    }

    destination_name_copy =
        evidence_copy_duplicate_string(
            destination_name,
            error
        );

    if (destination_name_copy == NULL)
    {
        goto cleanup;
    }

    result =
        g_try_new0(
            EvidenceCopyResult,
            1
        );

    if (result == NULL)
    {
        evidence_copy_set_error_literal(
            error,
            EVIDENCE_COPY_ERROR_MEMORY,
            "Impossible d’allouer le résultat de la copie."
        );

        goto cleanup;
    }

    result->destination_path =
        destination_path;

    destination_path = NULL;

    result->destination_name =
        destination_name_copy;

    destination_name_copy = NULL;

    result->sha256 =
        destination_sha256;

    destination_sha256 = NULL;

    result->size_bytes =
        destination_size;

    success = TRUE;

cleanup:

    if (destination_descriptor >= 0)
    {
        if (close(
                destination_descriptor
            ) != 0)
        {
            g_warning(
                "Impossible de fermer le fichier destination '%s' : %s",
                destination_path != NULL
                    ? destination_path
                    : destination_name,
                g_strerror(errno)
            );
        }
    }

    if (source_descriptor >= 0)
    {
        if (close(
                source_descriptor
            ) != 0)
        {
            g_warning(
                "Impossible de fermer le fichier source '%s' : %s",
                source_path,
                g_strerror(errno)
            );
        }
    }

    /*
     * Une destination n’est conservée qu’après validation complète.
     */
    if (!success &&
        destination_created &&
        destination_directory_descriptor >= 0)
    {
        if (evidence_copy_remove_invalid_destination(
            destination_directory_descriptor,
            destination_name,
            destination_path != NULL
                ? destination_path
                : destination_name
        ) != 0)
        {
            cleanup_errno = errno;

            evidence_copy_report_cleanup_failure(
                error,
                destination_path != NULL
                    ? destination_path
                    : destination_name,
                cleanup_errno
            );
        }
    }

    if (destination_directory_descriptor >= 0)
    {
        if (close(
                destination_directory_descriptor
            ) != 0)
        {
            g_warning(
                "Impossible de fermer le dossier destination '%s' : %s",
                destination_directory,
                g_strerror(errno)
            );
        }
    }

    g_free(
        destination_name_copy
    );

    g_free(
        destination_path
    );

    g_free(
        destination_sha256
    );

    g_free(
        source_sha256_after
    );

    g_free(
        source_sha256_before
    );

    if (!success)
    {
        evidence_copy_result_free(
            result
        );

        result = NULL;
    }

    return result;
}

void evidence_copy_result_free(
    EvidenceCopyResult *result
)
{
    if (result == NULL)
    {
        return;
    }

    g_free(
        result->sha256
    );

    g_free(
        result->destination_name
    );

    g_free(
        result->destination_path
    );

    g_free(
        result
    );
}

const char *evidence_copy_result_get_destination_path(
    const EvidenceCopyResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->destination_path;
}

const char *evidence_copy_result_get_destination_name(
    const EvidenceCopyResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->destination_name;
}

const char *evidence_copy_result_get_sha256(
    const EvidenceCopyResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->sha256;
}

guint64 evidence_copy_result_get_size_bytes(
    const EvidenceCopyResult *result
)
{
    if (result == NULL)
    {
        return 0;
    }

    return result->size_bytes;
}
