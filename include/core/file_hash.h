/******************************************************************************
 * @file file_hash.h
 * @brief Calcul des empreintes cryptographiques des fichiers de preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_FILE_HASH_H
#define LABFY_INVESTIGATION_FILE_HASH_H

#include <gio/gio.h>
#include <glib.h>

/**
 * @brief Catégories d’erreurs du module FileHash.
 */
typedef enum
{
    FILE_HASH_ERROR_INVALID_ARGUMENT,
    FILE_HASH_ERROR_NOT_FOUND,
    FILE_HASH_ERROR_NOT_REGULAR,
    FILE_HASH_ERROR_OPEN,
    FILE_HASH_ERROR_READ,
    FILE_HASH_ERROR_CANCELLED,
    FILE_HASH_ERROR_MEMORY
} FileHashError;

/**
 * @brief Domaine d’erreurs du module FileHash.
 */
#define FILE_HASH_ERROR \
    file_hash_error_quark()

/**
 * @brief Retourne le domaine d’erreurs du module FileHash.
 */
GQuark file_hash_error_quark(void);

/**
 * @brief Calcule le SHA-256 et la taille d’un fichier régulier.
 *
 * Le fichier est lu progressivement et n’est jamais chargé intégralement
 * en mémoire.
 *
 * Le chemin final ne doit pas être un lien symbolique.
 *
 * Avant l’appel :
 *
 * - out_sha256 doit être valide ;
 * - *out_sha256 doit valoir NULL ;
 * - out_size_bytes doit être valide ;
 * - error doit être NULL ou pointer vers NULL.
 *
 * Après un succès :
 *
 * - *out_sha256 contient une chaîne de 64 caractères hexadécimaux
 *   minuscules, à libérer avec g_free() ;
 * - *out_size_bytes contient le nombre d’octets effectivement lus.
 *
 * Après un échec :
 *
 * - *out_sha256 reste NULL ;
 * - *out_size_bytes vaut 0.
 *
 * @param file_path Chemin du fichier à analyser.
 * @param cancellable Objet d’annulation facultatif.
 * @param out_sha256 Destination de la chaîne SHA-256.
 * @param out_size_bytes Destination de la taille du fichier.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return TRUE en cas de succès, sinon FALSE.
 */
gboolean file_hash_compute_sha256(
    const char *file_path,
    GCancellable *cancellable,
    char **out_sha256,
    guint64 *out_size_bytes,
    GError **error
);

#ifdef FILE_HASH_ENABLE_TEST_HOOKS

/**
 * @brief Crochet appelé après chaque bloc lu, réservé aux tests.
 */
typedef void (*FileHashTestBlockHook)(
    guint64 total_size_bytes,
    gpointer user_data
);

/**
 * @brief Configure le crochet de lecture réservé aux tests.
 */
void file_hash_test_set_block_hook(
    FileHashTestBlockHook block_hook,
    gpointer user_data
);

#endif

#endif
