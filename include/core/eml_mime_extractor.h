/******************************************************************************
 * @file eml_mime_extractor.h
 * @brief Extraction MIME sécurisée et inventaire des pièces jointes d'un EML.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EML_MIME_EXTRACTOR_H
#define LABFY_INVESTIGATION_EML_MIME_EXTRACTOR_H

#include <gio/gio.h>

G_BEGIN_DECLS

/** @brief Limites de sécurité centralisées de l'extracteur MIME. */
#define EML_MIME_MAX_FILE_SIZE (50U * 1024U * 1024U)
#define EML_MIME_MAX_PART_DECODED_SIZE (8U * 1024U * 1024U)
#define EML_MIME_MAX_TOTAL_DECODED_SIZE (32U * 1024U * 1024U)
#define EML_MIME_MAX_PARTS 128U
#define EML_MIME_MAX_DEPTH 12U
#define EML_MIME_MAX_FILENAME_LENGTH 240U
#define EML_MIME_MAX_HEADER_VALUE_LENGTH (64U * 1024U)

/** @brief Représentation d'une pièce jointe extraite d'un message EML. */
typedef struct EmlAttachment
{
    char *part_index;           /**< Chemin/index MIME (ex: "1.2") */
    char *declared_filename;    /**< Nom de fichier d'origine */
    char *decoded_filename;     /**< Nom déclaré décodé RFC 2047/2231 */
    char *sanitized_filename;   /**< Nom assaini (anti path-traversal) */
    char *extracted_path;       /**< Chemin absolu dans 02_Preuves_Traitees */
    char *relative_path;        /**< Chemin relatif par rapport à la racine d'enquête */
    char *content_type;         /**< Type MIME déclaré */
    char *detected_mime;        /**< Type MIME détecté */
    char *content_id;           /**< Content-ID pour les images/pièces inline */
    char *normalized_content_id;/**< Content-ID sans chevrons */
    char *content_disposition;  /**< Content-Disposition brut de la partie */
    char *normalized_disposition; /**< attachment, inline ou NULL */
    char *transfer_encoding;    /**< Content-Transfer-Encoding */
    char *extracted_at_utc;     /**< Date UTC de l'extraction */
    gboolean is_inline;         /**< VRAI si disposition inline */
    gboolean is_attachment;     /**< VRAI si disposition attachment */
    gboolean is_truncated;      /**< VRAI si le contenu a dû être tronqué */
    gsize encoded_size;         /**< Taille encodée */
    gsize decoded_size;         /**< Taille décodée */
    char *sha256;               /**< Empreinte SHA-256 du fichier extrait */
    gboolean has_inconsistency; /**< VRAI si incohérence extension/MIME */
} EmlAttachment;

/** @brief Résultat de l'extraction MIME d'un fichier EML. */
typedef struct EmlMimeResult
{
    GPtrArray *attachments;     /**< Tableau de EmlAttachment* */
    GPtrArray *warnings;        /**< Avertissements d'extraction */
} EmlMimeResult;

/**
 * @brief Libère une structure EmlAttachment.
 */
void eml_attachment_free(EmlAttachment *attachment);

/**
 * @brief Libère une structure EmlMimeResult.
 */
void eml_mime_result_free(EmlMimeResult *result);

/**
 * @brief Assainit un nom de fichier pour éviter les attaques par traversée de chemin.
 * @param raw_filename Nom d'origine.
 * @return Nom assaini libéré avec g_free.
 */
char *eml_mime_sanitize_filename(const char *raw_filename);

/**
 * @brief Extrait et enregistre de manière sécurisée les pièces jointes d'un fichier EML.
 * @param eml_path Chemin du fichier EML source.
 * @param target_dir Dossier de destination dans 02_Preuves_Traitees (ex: .../02_Preuves_Traitees/eml_attachments/<hash>).
 * @param error Destination d'erreur facultative.
 * @return Résultat MIME, ou NULL en cas d'erreur.
 */
EmlMimeResult *eml_mime_extract_attachments(const char *eml_path,
                                             const char *target_dir,
                                             GError **error);

/**
 * @brief Variante annulable de l'extraction MIME.
 *
 * @param eml_path Chemin du fichier EML source.
 * @param target_dir Dossier de destination.
 * @param cancellable Objet d'annulation facultatif.
 * @param error Destination d'erreur facultative.
 * @return Résultat MIME, ou NULL en cas d'erreur.
 */
EmlMimeResult *eml_mime_extract_attachments_cancellable(
    const char *eml_path,
    const char *target_dir,
    GCancellable *cancellable,
    GError **error
);

G_END_DECLS

#endif /* LABFY_INVESTIGATION_EML_MIME_EXTRACTOR_H */
