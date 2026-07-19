/******************************************************************************
 * @file evidence_importer.h
 * @brief Orchestration de l’import transactionnel des preuves.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_IMPORTER_H
#define LABFY_INVESTIGATION_EVIDENCE_IMPORTER_H

#include "database/database.h"
#include "models/evidence_record.h"

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Catégories d’erreurs du module EvidenceImporter.
 */
typedef enum
{
    EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT,
    EVIDENCE_IMPORTER_ERROR_MEMORY,
    EVIDENCE_IMPORTER_ERROR_CANCELLED,
    EVIDENCE_IMPORTER_ERROR_DESTINATION,
    EVIDENCE_IMPORTER_ERROR_COPY,
    EVIDENCE_IMPORTER_ERROR_MODEL,
    EVIDENCE_IMPORTER_ERROR_DATABASE,
    EVIDENCE_IMPORTER_ERROR_ROLLBACK
} EvidenceImporterError;

/**
 * @brief Domaine d’erreurs du module EvidenceImporter.
 */
#define EVIDENCE_IMPORTER_ERROR \
    evidence_importer_error_quark()

/**
 * @brief Retourne le domaine d’erreurs du module.
 */
GQuark evidence_importer_error_quark(void);

/**
 * @brief Service opaque orchestrant l’import des preuves.
 *
 * L’objet emprunte la connexion Database reçue à sa création.
 */
typedef struct EvidenceImporter EvidenceImporter;

/**
 * @brief Paramètres empruntés d’un import de preuve.
 *
 * Les chaînes restent la propriété de l’appelant.
 *
 * source_path, destination_directory, relative_directory et
 * type_identifier sont obligatoires.
 *
 * collected_at, source et description sont facultatifs.
 */
typedef struct
{
    const char *source_path;
    const char *destination_directory;
    const char *relative_directory;
    const char *type_identifier;

    const char *collected_at;
    const char *source;
    const char *description;
} EvidenceImportRequest;

/**
 * @brief Crée un service d’import utilisant une connexion existante.
 *
 * La connexion est empruntée et doit rester valide pendant toute la durée
 * de vie de l’importeur.
 *
 * @param database Connexion ouverte et migrée.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouvel importeur, ou NULL.
 */
EvidenceImporter *evidence_importer_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère un importeur.
 *
 * La connexion Database empruntée n’est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param evidence_importer Importeur à libérer.
 */
void evidence_importer_free(
    EvidenceImporter *evidence_importer
);

/**
 * @brief Importe une preuve de manière transactionnelle.
 *
 * Après succès :
 *
 * - le fichier est présent dans l’enquête ;
 * - le modèle est enregistré dans SQLite ;
 * - le EvidenceRecord retourné appartient à l’appelant.
 *
 * Après échec :
 *
 * - aucun EvidenceRecord n’est retourné ;
 * - aucune transaction démarrée par l’importeur ne reste active ;
 * - toute copie créée par l’opération est supprimée lorsque cela est
 *   encore possible.
 *
 * @param evidence_importer Service d’import.
 * @param request Paramètres empruntés de l’import.
 * @param cancellable Objet d’annulation facultatif.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouveau EvidenceRecord, ou NULL.
 */
EvidenceRecord *evidence_importer_import(
    EvidenceImporter *evidence_importer,
    const EvidenceImportRequest *request,
    GCancellable *cancellable,
    GError **error
);

G_END_DECLS

#endif
