/******************************************************************************
 * @file file_hash.c
 * @brief Calcul progressif du SHA-256 des fichiers de preuve.
 ******************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include "core/file_hash.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Taille du tampon de lecture.
 */
#define FILE_HASH_BLOCK_SIZE (64U * 1024U)

/**
 * @brief Nombre d’octets composant un SHA-256.
 */
#define FILE_HASH_SHA256_DIGEST_SIZE 32U

/**
 * @brief Nombre de caractères hexadécimaux composant un SHA-256.
 */
#define FILE_HASH_SHA256_TEXT_SIZE 64U

#ifdef FILE_HASH_ENABLE_TEST_HOOKS

static FileHashTestBlockHook file_hash_test_block_hook = NULL;
static gpointer file_hash_test_block_hook_data = NULL;

void file_hash_test_set_block_hook(
    FileHashTestBlockHook block_hook,
    gpointer user_data
)
{
    file_hash_test_block_hook = block_hook;
    file_hash_test_block_hook_data = user_data;
}

static void file_hash_test_notify_block(
    guint64 total_size_bytes
)
{
    if (file_hash_test_block_hook == NULL)
    {
        return;
    }

    file_hash_test_block_hook(
        total_size_bytes,
        file_hash_test_block_hook_data
    );
}

#else

static void file_hash_test_notify_block(
    guint64 total_size_bytes
)
{
    /*
     * Le paramètre est volontairement inutilisé dans la version
     * de production.
     */
    (void) total_size_bytes;
}

#endif

/**
 * @brief Enregistre une erreur littérale.
 */
static void file_hash_set_error_literal(
    GError **error,
    FileHashError error_code,
    const char *error_message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        FILE_HASH_ERROR,
        error_code,
        error_message
    );
}

/**
 * @brief Enregistre une erreur contenant un chemin et une erreur système.
 */
static void file_hash_set_system_error(
    GError **error,
    FileHashError error_code,
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
        FILE_HASH_ERROR,
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
static gboolean file_hash_check_cancelled(
    GCancellable *cancellable,
    GError **error
)
{
    if (cancellable == NULL ||
        !g_cancellable_is_cancelled(cancellable))
    {
        return FALSE;
    }

    file_hash_set_error_literal(
        error,
        FILE_HASH_ERROR_CANCELLED,
        "Le calcul du SHA-256 a été annulé."
    );

    return TRUE;
}

/**
 * @brief Convertit le condensat SHA-256 en texte hexadécimal minuscule.
 */
static char *file_hash_digest_to_hex(
    const guint8 *digest,
    gsize digest_size,
    GError **error
)
{
    static const char hexadecimal_digits[] =
        "0123456789abcdef";

    char *sha256 = NULL;
    gsize digest_index = 0;

    if (digest == NULL ||
        digest_size != FILE_HASH_SHA256_DIGEST_SIZE)
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_INVALID_ARGUMENT,
            "Le condensat SHA-256 est invalide."
        );

        return NULL;
    }

    sha256 = g_try_malloc0(
        FILE_HASH_SHA256_TEXT_SIZE + 1U
    );

    if (sha256 == NULL)
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_MEMORY,
            "Impossible d’allouer la chaîne SHA-256."
        );

        return NULL;
    }

    for (digest_index = 0;
         digest_index < digest_size;
         digest_index++)
    {
        sha256[digest_index * 2U] =
            hexadecimal_digits[
                digest[digest_index] >> 4U
            ];

        sha256[digest_index * 2U + 1U] =
            hexadecimal_digits[
                digest[digest_index] & 0x0FU
            ];
    }

    return sha256;
}

GQuark file_hash_error_quark(void)
{
    return g_quark_from_static_string(
        "file-hash-error-quark"
    );
}

gboolean file_hash_compute_sha256(
    const char *file_path,
    GCancellable *cancellable,
    char **out_sha256,
    guint64 *out_size_bytes,
    GError **error
)
{
    GChecksum *checksum = NULL;

    guint8 *read_buffer = NULL;
    guint8 digest[FILE_HASH_SHA256_DIGEST_SIZE];

    struct stat file_status;

    char *sha256 = NULL;

    gsize digest_size =
        FILE_HASH_SHA256_DIGEST_SIZE;

    guint64 total_size_bytes = 0;

    ssize_t read_size = 0;

    int file_descriptor = -1;
    int saved_errno = 0;

    gboolean success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (out_size_bytes != NULL)
    {
        *out_size_bytes = 0;
    }

    if (file_path == NULL ||
        file_path[0] == '\0' ||
        out_sha256 == NULL ||
        out_size_bytes == NULL)
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_INVALID_ARGUMENT,
            "Les paramètres du calcul SHA-256 sont invalides."
        );

        return FALSE;
    }

    /*
     * Le module ne doit jamais libérer une ancienne chaîne appartenant
     * à l’appelant.
     */
    if (*out_sha256 != NULL)
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_INVALID_ARGUMENT,
            "La destination SHA-256 doit initialement valoir NULL."
        );

        return FALSE;
    }

    if (file_hash_check_cancelled(
            cancellable,
            error
        ))
    {
        return FALSE;
    }

    /*
     * O_NOFOLLOW refuse un lien symbolique utilisé comme dernier composant.
     *
     * O_NONBLOCK évite de rester bloqué lors de l’ouverture accidentelle
     * d’un tube nommé ou d’un fichier spécial.
     */
    file_descriptor = open(
        file_path,
        O_RDONLY |
        O_CLOEXEC |
        O_NOFOLLOW |
        O_NONBLOCK
    );

    if (file_descriptor < 0)
    {
        saved_errno = errno;

        if (saved_errno == ENOENT ||
            saved_errno == ENOTDIR)
        {
            file_hash_set_system_error(
                error,
                FILE_HASH_ERROR_NOT_FOUND,
                "Le fichier est introuvable",
                file_path,
                saved_errno
            );
        }
        else if (saved_errno == ELOOP)
        {
            file_hash_set_error_literal(
                error,
                FILE_HASH_ERROR_NOT_REGULAR,
                "Le chemin désigne un lien symbolique."
            );
        }
        else
        {
            file_hash_set_system_error(
                error,
                FILE_HASH_ERROR_OPEN,
                "Impossible d’ouvrir le fichier",
                file_path,
                saved_errno
            );
        }

        goto cleanup;
    }

    if (fstat(
            file_descriptor,
            &file_status
        ) != 0)
    {
        saved_errno = errno;

        file_hash_set_system_error(
            error,
            FILE_HASH_ERROR_OPEN,
            "Impossible d’inspecter le fichier",
            file_path,
            saved_errno
        );

        goto cleanup;
    }

    if (!S_ISREG(file_status.st_mode))
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_NOT_REGULAR,
            "Le chemin ne désigne pas un fichier régulier."
        );

        goto cleanup;
    }

    if (file_hash_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    checksum = g_checksum_new(
        G_CHECKSUM_SHA256
    );

    if (checksum == NULL)
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_MEMORY,
            "Impossible d’allouer le calculateur SHA-256."
        );

        goto cleanup;
    }

    read_buffer = g_try_malloc(
        FILE_HASH_BLOCK_SIZE
    );

    if (read_buffer == NULL)
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_MEMORY,
            "Impossible d’allouer le tampon de lecture."
        );

        goto cleanup;
    }

    while (TRUE)
    {
        if (file_hash_check_cancelled(
                cancellable,
                error
            ))
        {
            goto cleanup;
        }

        read_size = read(
            file_descriptor,
            read_buffer,
            FILE_HASH_BLOCK_SIZE
        );

        if (read_size < 0)
        {
            saved_errno = errno;

            if (saved_errno == EINTR)
            {
                continue;
            }

            file_hash_set_system_error(
                error,
                FILE_HASH_ERROR_READ,
                "Impossible de lire le fichier",
                file_path,
                saved_errno
            );

            goto cleanup;
        }

        if (read_size == 0)
        {
            break;
        }

        g_checksum_update(
            checksum,
            read_buffer,
            (gssize) read_size
        );

        total_size_bytes +=
            (guint64) read_size;

        file_hash_test_notify_block(
            total_size_bytes
        );

        if (file_hash_check_cancelled(
                cancellable,
                error
            ))
        {
            goto cleanup;
        }
    }

    if (file_hash_check_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    g_checksum_get_digest(
        checksum,
        digest,
        &digest_size
    );

    if (digest_size !=
        FILE_HASH_SHA256_DIGEST_SIZE)
    {
        file_hash_set_error_literal(
            error,
            FILE_HASH_ERROR_READ,
            "La taille du condensat SHA-256 est invalide."
        );

        goto cleanup;
    }

    sha256 = file_hash_digest_to_hex(
        digest,
        digest_size,
        error
    );

    if (sha256 == NULL)
    {
        goto cleanup;
    }

    /*
     * Les sorties ne sont publiées qu’après le succès complet.
     */
    *out_sha256 = sha256;
    *out_size_bytes = total_size_bytes;

    sha256 = NULL;
    success = TRUE;

cleanup:

    g_free(
        sha256
    );

    g_free(
        read_buffer
    );

    if (checksum != NULL)
    {
        g_checksum_free(
            checksum
        );
    }

    if (file_descriptor >= 0)
    {
        if (close(
                file_descriptor
            ) != 0 &&
            success)
        {
            /*
             * Une erreur de fermeture en lecture seule ne modifie pas
             * le condensat déjà calculé. Elle est néanmoins signalée.
             */
            g_warning(
                "Impossible de fermer proprement le fichier '%s' : %s",
                file_path,
                g_strerror(errno)
            );
        }
    }

    return success;
}
