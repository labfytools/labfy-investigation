/******************************************************************************
 * @file evidence_integrity_verifier.c
 * @brief Vérification de l'intégrité d'une preuve numérique.
 ******************************************************************************/

#define _XOPEN_SOURCE 700

#include "core/evidence_integrity_verifier.h"

#include "core/file_hash.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Résultat interne d'une vérification.
 */
struct EvidenceIntegrityVerificationResult
{
    EvidenceIntegrityStatus status;

    char *computed_sha256;
    guint64 size_bytes;

    char *diagnostic;
};

/**
 * @brief Enregistre une erreur technique.
 */
static void evidence_integrity_verifier_set_error_literal(
    GError **error,
    EvidenceIntegrityVerifierError error_code,
    const char *message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_INTEGRITY_VERIFIER_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Vérifie qu'une empreinte SHA-256 est canonique.
 */
static gboolean evidence_integrity_verifier_sha256_is_valid(
    const char *sha256
)
{
    gsize character_index = 0;

    if (sha256 == NULL ||
        strlen(sha256) != 64U)
    {
        return FALSE;
    }

    for (character_index = 0;
         character_index < 64U;
         character_index++)
    {
        if (!g_ascii_isdigit(sha256[character_index]) &&
            (
                sha256[character_index] < 'a' ||
                sha256[character_index] > 'f'
            ))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Crée un résultat métier.
 */
static EvidenceIntegrityVerificationResult *
evidence_integrity_verification_result_new(
    EvidenceIntegrityStatus status,
    const char *computed_sha256,
    guint64 size_bytes,
    const char *diagnostic,
    GError **error
)
{
    EvidenceIntegrityVerificationResult *result =
        NULL;

    result =
        g_try_new0(
            EvidenceIntegrityVerificationResult,
            1
        );

    if (result == NULL)
    {
        evidence_integrity_verifier_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR_MEMORY,
            "Impossible d'allouer le résultat de la vérification."
        );

        return NULL;
    }

    result->status =
        status;

    result->size_bytes =
        size_bytes;

    if (computed_sha256 != NULL)
    {
        result->computed_sha256 =
            g_strdup(
                computed_sha256
            );
    }

    if (diagnostic != NULL)
    {
        result->diagnostic =
            g_strdup(
                diagnostic
            );
    }

    return result;
}

/**
 * @brief Vérifie si une annulation a été demandée.
 */
static gboolean evidence_integrity_verifier_is_cancelled(
    GCancellable *cancellable,
    GError **error
)
{
    if (cancellable == NULL ||
        !g_cancellable_is_cancelled(
            cancellable
        ))
    {
        return FALSE;
    }

    evidence_integrity_verifier_set_error_literal(
        error,
        EVIDENCE_INTEGRITY_VERIFIER_ERROR_CANCELLED,
        "La vérification d'intégrité a été annulée."
    );

    return TRUE;
}

GQuark evidence_integrity_verifier_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-integrity-verifier-error-quark"
    );
}

EvidenceIntegrityVerificationResult *
evidence_integrity_verifier_verify(
    const char *investigation_root_path,
    const char *relative_path,
    const char *expected_sha256,
    GCancellable *cancellable,
    GError **error
)
{
    EvidenceIntegrityVerificationResult *result =
        NULL;

    GError *hash_error =
        NULL;

    char **path_components =
        NULL;

    char *resolved_root_path =
        NULL;

    char *current_path =
        NULL;

    char *next_path =
        NULL;

    char *computed_sha256 =
        NULL;

    char *diagnostic =
        NULL;

    struct stat root_status;
    struct stat path_status;

    guint64 size_bytes =
        0;

    guint component_index =
        0;

    gboolean is_last_component =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (investigation_root_path == NULL ||
        investigation_root_path[0] == '\0' ||
        relative_path == NULL ||
        relative_path[0] == '\0' ||
        !evidence_integrity_verifier_sha256_is_valid(
            expected_sha256
        ))
    {
        evidence_integrity_verifier_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la vérification sont invalides."
        );

        return NULL;
    }

    if (evidence_integrity_verifier_is_cancelled(
            cancellable,
            error
        ))
    {
        return NULL;
    }

    /*
     * La racine existe déjà lorsqu'une InvestigationSession est ouverte.
     * realpath() résout son éventuel lien symbolique et la canonicalise.
     */
    resolved_root_path =
        realpath(
            investigation_root_path,
            NULL
        );

    if (resolved_root_path == NULL)
    {
        g_set_error(
            error,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR_ROOT,
            "Impossible de résoudre la racine '%s' : %s",
            investigation_root_path,
            g_strerror(errno)
        );

        return NULL;
    }

    if (stat(
            resolved_root_path,
            &root_status
        ) != 0 ||
        !S_ISDIR(root_status.st_mode))
    {
        evidence_integrity_verifier_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR_ROOT,
            "La racine de l'enquête n'est pas un dossier valide."
        );

        goto cleanup;
    }

    /*
     * Un chemin SQLite doit toujours être relatif à l'enquête.
     */
    if (g_path_is_absolute(
            relative_path
        ))
    {
        result =
            evidence_integrity_verification_result_new(
                EVIDENCE_INTEGRITY_STATUS_ERROR,
                NULL,
                0,
                "Le chemin enregistré est absolu.",
                error
            );

        goto cleanup;
    }

    path_components =
        g_strsplit(
            relative_path,
            G_DIR_SEPARATOR_S,
            -1
        );

    if (path_components == NULL)
    {
        evidence_integrity_verifier_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR_MEMORY,
            "Impossible de décomposer le chemin de la preuve."
        );

        goto cleanup;
    }

    current_path =
        g_strdup(
            resolved_root_path
        );

    if (current_path == NULL)
    {
        evidence_integrity_verifier_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR_MEMORY,
            "Impossible de conserver le chemin de la racine."
        );

        goto cleanup;
    }

    for (component_index = 0;
         path_components[component_index] != NULL;
         component_index++)
    {
        const char *component =
            path_components[component_index];

        is_last_component =
            path_components[component_index + 1U] == NULL;

        /*
         * Le refus explicite évite toute traversée hors de la racine.
         */
        if (component[0] == '\0' ||
            g_strcmp0(component, ".") == 0 ||
            g_strcmp0(component, "..") == 0)
        {
            result =
                evidence_integrity_verification_result_new(
                    EVIDENCE_INTEGRITY_STATUS_ERROR,
                    NULL,
                    0,
                    "Le chemin contient un composant interdit.",
                    error
                );

            goto cleanup;
        }

        next_path =
            g_build_filename(
                current_path,
                component,
                NULL
            );

        if (next_path == NULL)
        {
            evidence_integrity_verifier_set_error_literal(
                error,
                EVIDENCE_INTEGRITY_VERIFIER_ERROR_MEMORY,
                "Impossible de construire le chemin de la preuve."
            );

            goto cleanup;
        }

        g_free(
            current_path
        );

        current_path =
            next_path;

        next_path =
            NULL;

        if (lstat(
                current_path,
                &path_status
            ) != 0)
        {
            if (errno == ENOENT ||
                errno == ENOTDIR)
            {
                result =
                    evidence_integrity_verification_result_new(
                        EVIDENCE_INTEGRITY_STATUS_MISSING,
                        NULL,
                        0,
                        "Le fichier de preuve est absent.",
                        error
                    );
            }
            else
            {
                diagnostic =
                    g_strdup_printf(
                        "Impossible d'inspecter '%s' : %s",
                        current_path,
                        g_strerror(errno)
                    );

                result =
                    evidence_integrity_verification_result_new(
                        EVIDENCE_INTEGRITY_STATUS_ERROR,
                        NULL,
                        0,
                        diagnostic,
                        error
                    );
            }

            goto cleanup;
        }

        /*
         * Tous les liens symboliques sont refusés, y compris lorsqu'ils
         * semblent rester à l'intérieur de l'enquête.
         */
        if (S_ISLNK(path_status.st_mode))
        {
            result =
                evidence_integrity_verification_result_new(
                    EVIDENCE_INTEGRITY_STATUS_ERROR,
                    NULL,
                    0,
                    "Le chemin de la preuve contient un lien symbolique.",
                    error
                );

            goto cleanup;
        }

        if (!is_last_component &&
            !S_ISDIR(path_status.st_mode))
        {
            result =
                evidence_integrity_verification_result_new(
                    EVIDENCE_INTEGRITY_STATUS_ERROR,
                    NULL,
                    0,
                    "Un composant intermédiaire du chemin "
                    "n'est pas un dossier.",
                    error
                );

            goto cleanup;
        }
    }

    if (evidence_integrity_verifier_is_cancelled(
            cancellable,
            error
        ))
    {
        goto cleanup;
    }

    if (!file_hash_compute_sha256(
            current_path,
            cancellable,
            &computed_sha256,
            &size_bytes,
            &hash_error
        ))
    {
        if (g_error_matches(
                hash_error,
                FILE_HASH_ERROR,
                FILE_HASH_ERROR_CANCELLED
            ))
        {
            evidence_integrity_verifier_set_error_literal(
                error,
                EVIDENCE_INTEGRITY_VERIFIER_ERROR_CANCELLED,
                "La vérification d'intégrité a été annulée."
            );

            goto cleanup;
        }

        if (g_error_matches(
                hash_error,
                FILE_HASH_ERROR,
                FILE_HASH_ERROR_NOT_FOUND
            ))
        {
            result =
                evidence_integrity_verification_result_new(
                    EVIDENCE_INTEGRITY_STATUS_MISSING,
                    NULL,
                    0,
                    hash_error->message,
                    error
                );

            goto cleanup;
        }

        result =
            evidence_integrity_verification_result_new(
                EVIDENCE_INTEGRITY_STATUS_ERROR,
                NULL,
                0,
                hash_error != NULL
                    ? hash_error->message
                    : "Le calcul SHA-256 a échoué.",
                error
            );

        goto cleanup;
    }

    if (strcmp(
            computed_sha256,
            expected_sha256
        ) == 0)
    {
        result =
            evidence_integrity_verification_result_new(
                EVIDENCE_INTEGRITY_STATUS_VALID,
                computed_sha256,
                size_bytes,
                NULL,
                error
            );
    }
    else
    {
        result =
            evidence_integrity_verification_result_new(
                EVIDENCE_INTEGRITY_STATUS_MODIFIED,
                computed_sha256,
                size_bytes,
                "L'empreinte recalculée diffère "
                "de l'empreinte enregistrée.",
                error
            );
    }

cleanup:

    g_clear_error(
        &hash_error
    );

    g_free(
        diagnostic
    );

    g_free(
        computed_sha256
    );

    g_free(
        next_path
    );

    g_free(
        current_path
    );

    g_strfreev(
        path_components
    );

    free(
        resolved_root_path
    );

    return result;
}

EvidenceIntegrityStatus
evidence_integrity_verification_result_get_status(
    const EvidenceIntegrityVerificationResult *result
)
{
    if (result == NULL)
    {
        return EVIDENCE_INTEGRITY_STATUS_ERROR;
    }

    return result->status;
}

const char *
evidence_integrity_verification_result_get_computed_sha256(
    const EvidenceIntegrityVerificationResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->computed_sha256;
}

guint64 evidence_integrity_verification_result_get_size_bytes(
    const EvidenceIntegrityVerificationResult *result
)
{
    if (result == NULL)
    {
        return 0;
    }

    return result->size_bytes;
}

const char *
evidence_integrity_verification_result_get_diagnostic(
    const EvidenceIntegrityVerificationResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->diagnostic;
}

void evidence_integrity_verification_result_free(
    EvidenceIntegrityVerificationResult *result
)
{
    if (result == NULL)
    {
        return;
    }

    g_free(
        result->diagnostic
    );

    g_free(
        result->computed_sha256
    );

    g_free(
        result
    );
}
