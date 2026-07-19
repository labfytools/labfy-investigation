/******************************************************************************
 * @file evidence_copy.h
 * @brief Copie sûre et vérifiée des fichiers de preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_COPY_H
#define LABFY_INVESTIGATION_EVIDENCE_COPY_H

#include <gio/gio.h>
#include <glib.h>

/**
 * @brief Catégories d’erreurs du module EvidenceCopy.
 */
typedef enum
{
    EVIDENCE_COPY_ERROR_INVALID_ARGUMENT,
    EVIDENCE_COPY_ERROR_SOURCE_NOT_FOUND,
    EVIDENCE_COPY_ERROR_SOURCE_NOT_REGULAR,
    EVIDENCE_COPY_ERROR_DESTINATION_INVALID,
    EVIDENCE_COPY_ERROR_DESTINATION_EXISTS,
    EVIDENCE_COPY_ERROR_OPEN_SOURCE,
    EVIDENCE_COPY_ERROR_CREATE_DESTINATION,
    EVIDENCE_COPY_ERROR_READ,
    EVIDENCE_COPY_ERROR_WRITE,
    EVIDENCE_COPY_ERROR_SYNC,
    EVIDENCE_COPY_ERROR_HASH,
    EVIDENCE_COPY_ERROR_VERIFY,
    EVIDENCE_COPY_ERROR_CANCELLED,
    EVIDENCE_COPY_ERROR_MEMORY,
    EVIDENCE_COPY_ERROR_CLEANUP
} EvidenceCopyError;

/**
 * @brief Domaine d’erreurs du module EvidenceCopy.
 */
#define EVIDENCE_COPY_ERROR \
    evidence_copy_error_quark()

/**
 * @brief Retourne le domaine d’erreurs du module EvidenceCopy.
 */
GQuark evidence_copy_error_quark(void);

/**
 * @brief Résultat opaque d’une copie de preuve validée.
 */
typedef struct EvidenceCopyResult EvidenceCopyResult;

/**
 * @brief Copie et vérifie un fichier de preuve.
 *
 * La fonction :
 *
 * - refuse les sources non régulières ;
 * - refuse un dossier destination symbolique ;
 * - crée exclusivement un nouveau fichier ;
 * - copie le contenu progressivement ;
 * - synchronise les données écrites ;
 * - compare la taille et le SHA-256 de la source et de la destination ;
 * - supprime la destination en cas d’échec après sa création.
 *
 * Le fichier source n’est jamais modifié.
 *
 * Le nom destination doit représenter uniquement un nom de fichier :
 * il ne doit contenir aucun séparateur de chemin.
 *
 * @param source_path Chemin du fichier source.
 * @param destination_directory Dossier recevant la copie.
 * @param destination_name Nom du fichier destination.
 * @param cancellable Objet d’annulation facultatif.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouveau résultat possédé par l’appelant, ou NULL.
 */
EvidenceCopyResult *evidence_copy_file(
    const char *source_path,
    const char *destination_directory,
    const char *destination_name,
    GCancellable *cancellable,
    GError **error
);

/**
 * @brief Libère un résultat de copie.
 *
 * La copie validée présente sur le disque n’est pas supprimée.
 * Cette fonction accepte NULL.
 *
 * @param result Résultat à libérer.
 */
void evidence_copy_result_free(
    EvidenceCopyResult *result
);

/**
 * @brief Retourne le chemin complet de la copie validée.
 *
 * Le pointeur appartient au résultat.
 *
 * @return Chemin destination, ou NULL.
 */
const char *evidence_copy_result_get_destination_path(
    const EvidenceCopyResult *result
);

/**
 * @brief Retourne le nom du fichier destination.
 *
 * Le pointeur appartient au résultat.
 *
 * @return Nom destination, ou NULL.
 */
const char *evidence_copy_result_get_destination_name(
    const EvidenceCopyResult *result
);

/**
 * @brief Retourne le SHA-256 validé de la copie.
 *
 * Le pointeur appartient au résultat.
 *
 * @return SHA-256 sur 64 caractères, ou NULL.
 */
const char *evidence_copy_result_get_sha256(
    const EvidenceCopyResult *result
);

/**
 * @brief Retourne la taille validée de la copie.
 *
 * @return Taille en octets, ou zéro si result vaut NULL.
 */
guint64 evidence_copy_result_get_size_bytes(
    const EvidenceCopyResult *result
);

#ifdef EVIDENCE_COPY_ENABLE_TEST_HOOKS

/**
 * @brief Crochet appelé après chaque bloc copié.
 */
typedef void (*EvidenceCopyTestBlockHook)(
    guint64 copied_size_bytes,
    gpointer user_data
);

void evidence_copy_test_set_block_hook(
    EvidenceCopyTestBlockHook block_hook,
    gpointer user_data
);

/**
 * @brief Crochet appelé avant chaque tentative d’écriture.
 */
typedef gboolean (*EvidenceCopyTestBeforeWriteHook)(
    guint64 written_size_bytes,
    gpointer user_data
);

void evidence_copy_test_set_before_write_hook(
    EvidenceCopyTestBeforeWriteHook before_write_hook,
    gpointer user_data
);

/**
 * @brief Crochet appelé avant la vérification cryptographique finale.
 */
typedef void (*EvidenceCopyTestBeforeVerifyHook)(
    const char *destination_path,
    gpointer user_data
);

void evidence_copy_test_set_before_verify_hook(
    EvidenceCopyTestBeforeVerifyHook before_verify_hook,
    gpointer user_data
);

/**
 * @brief Crochet appelé avant la suppression d’une copie invalide.
 *
 * Retourner TRUE simule un échec de suppression.
 */
typedef gboolean (*EvidenceCopyTestCleanupHook)(
    const char *destination_path,
    gpointer user_data
);

/**
 * @brief Configure le crochet de nettoyage réservé aux tests.
 */
void evidence_copy_test_set_cleanup_hook(
    EvidenceCopyTestCleanupHook cleanup_hook,
    gpointer user_data
);

#endif

#endif
