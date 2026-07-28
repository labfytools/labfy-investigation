#ifndef LABFY_INVESTIGATION_PERSON_CREATION_COORDINATOR_H
#define LABFY_INVESTIGATION_PERSON_CREATION_COORDINATOR_H

#include "core/person_entity_service.h"
#include "models/person_evidence_selection.h"
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
    char *person_identifier;
    GPtrArray *evidence_identifiers;
} PersonCreationCoordinatorResult;

PersonCreationCoordinatorResult *person_creation_coordinator_execute(
    Database *database, const char *investigation_root_path,
    const PersonEntityInput *person,
    const PersonEvidenceSelection *selection,
    GCancellable *cancellable, GError **error);
void person_creation_coordinator_result_free(
    PersonCreationCoordinatorResult *result);

G_END_DECLS
#endif
