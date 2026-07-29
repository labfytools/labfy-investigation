#ifndef LABFY_INVESTIGATION_PERSON_CREATION_COORDINATOR_H
#define LABFY_INVESTIGATION_PERSON_CREATION_COORDINATOR_H

#include "core/person_entity_service.h"
#include "models/person_evidence_selection.h"
#include "models/identity_ocr.h"
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
    char *person_identifier;
    GPtrArray *evidence_identifiers;
} PersonCreationCoordinatorResult;

typedef enum {
    PERSON_CREATION_FAILURE_NONE,
    PERSON_CREATION_FAILURE_VALIDATE,
    PERSON_CREATION_FAILURE_SESSION_BEFORE_START,
    PERSON_CREATION_FAILURE_SOURCE_HASH,
    PERSON_CREATION_FAILURE_CREATE_OCR_DIRECTORY,
    PERSON_CREATION_FAILURE_COPY_OCR_TEXT,
    PERSON_CREATION_FAILURE_HASH_OCR_TEXT,
    PERSON_CREATION_FAILURE_COPY_OCR_TSV,
    PERSON_CREATION_FAILURE_HASH_OCR_TSV,
    PERSON_CREATION_FAILURE_CREATE_PERSON,
    PERSON_CREATION_FAILURE_CREATE_ROLE,
    PERSON_CREATION_FAILURE_IMPORT_EVIDENCE,
    PERSON_CREATION_FAILURE_LINK_EVIDENCE,
    PERSON_CREATION_FAILURE_INSERT_OCR_RUN,
    PERSON_CREATION_FAILURE_INSERT_DOCUMENT_OBSERVATION,
    PERSON_CREATION_FAILURE_INSERT_FIELD,
    PERSON_CREATION_FAILURE_CREATE_SOURCE,
    PERSON_CREATION_FAILURE_SESSION_BEFORE_COMMIT,
    PERSON_CREATION_FAILURE_ARTIFACT_TEXT_CHANGED,
    PERSON_CREATION_FAILURE_ARTIFACT_TSV_CHANGED,
    PERSON_CREATION_FAILURE_COMMIT,
    PERSON_CREATION_FAILURE_COMPENSATION
} PersonCreationFailurePoint;
typedef gboolean (*PersonCreationSessionCheck)(gpointer user_data);
typedef struct {
    PersonCreationFailurePoint failure_point;
    guint failure_occurrence;
    gboolean inject_compensation_failure;
    PersonCreationSessionCheck session_check;
    gpointer session_check_data;
} PersonCreationCoordinatorOptions;
typedef struct {
    const char *collected_at;
    const char *source;
    const char *description;
    const char *type_identifier;
} PersonCreationCoordinatorEvidenceMetadata;

PersonCreationCoordinatorResult *person_creation_coordinator_execute(
    Database *database, const char *investigation_root_path,
    const PersonEntityInput *person,
    const PersonEvidenceSelection *selection,
    const GPtrArray *ocr_runs,
    GCancellable *cancellable, GError **error);
PersonCreationCoordinatorResult *person_creation_coordinator_execute_with_options(
    Database *database, const char *investigation_root_path,
    const PersonEntityInput *person,
    const PersonEvidenceSelection *selection,
    const GPtrArray *ocr_runs,
    const PersonCreationCoordinatorOptions *options,
    GCancellable *cancellable, GError **error);
PersonCreationCoordinatorResult *
person_creation_coordinator_attach_to_existing_person(
    Database *database, const char *investigation_root_path,
    const char *person_identifier,
    const PersonEvidenceSelection *selection,
    const GPtrArray *ocr_runs,
    const PersonCreationCoordinatorEvidenceMetadata *metadata,
    const PersonCreationCoordinatorOptions *options,
    GCancellable *cancellable, GError **error);
void person_creation_coordinator_result_free(
    PersonCreationCoordinatorResult *result);

G_END_DECLS
#endif
