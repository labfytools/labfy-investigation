/******************************************************************************
 * @file local_capability_registry.h
 * @brief Registre statique tools/adapters/capabilities du lot J4.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_LOCAL_CAPABILITY_REGISTRY_H
#define LABFY_INVESTIGATION_LOCAL_CAPABILITY_REGISTRY_H

#include "core/local_tool_runner.h"

G_BEGIN_DECLS

#define LOCAL_CAPABILITY_EML_HEADERS "labfy.capability.eml_headers.v1"
#define LOCAL_CAPABILITY_EXIF_METADATA "labfy.capability.exif_metadata.v1"

typedef enum { LOCAL_ADAPTER_NATIVE, LOCAL_ADAPTER_SUBPROCESS } LocalAdapterKind;
typedef enum {
    LOCAL_CAPABILITY_READY,
    LOCAL_CAPABILITY_MISSING,
    LOCAL_CAPABILITY_INCOMPATIBLE,
    LOCAL_CAPABILITY_UNVERIFIED,
    LOCAL_CAPABILITY_DETECTION_ERROR
} LocalCapabilityAvailability;

typedef struct {
    const char *capability_id;
    const char *capability_version;
    const char *adapter_id;
    const char *adapter_version;
    const char *tool_id;
    const char *intent_fr;
    const char *accepted_mime_prefix;
    const char *accepted_extension;
    const char *produced_type;
    const char *output_contract;
    LocalAdapterKind adapter_kind;
    const char *action_class;
    const char *network_contact;
    LocalToolRunnerProfile limits;
} LocalCapabilityDescriptor;

typedef struct {
    const LocalCapabilityDescriptor *descriptor;
    LocalCapabilityAvailability availability;
    char *reason;
    char *executable;
    char *tool_version;
} LocalCapabilityStatus;

typedef struct LocalCapabilityRegistry LocalCapabilityRegistry;

LocalCapabilityRegistry *local_capability_registry_new(GError **error);
void local_capability_registry_free(LocalCapabilityRegistry *registry);
const LocalCapabilityStatus *local_capability_registry_lookup(
    const LocalCapabilityRegistry *registry,
    const char *capability_id);
const GPtrArray *local_capability_registry_get_statuses(
    const LocalCapabilityRegistry *registry);

/** Les métadonnées MIME/extension sont contrôlées par l'EvidenceRecord. */
gboolean local_capability_status_applies(
    const LocalCapabilityStatus *status,
    const char *mime_type,
    const char *relative_path,
    char **out_reason);
const char *local_capability_availability_code(
    LocalCapabilityAvailability availability);

G_END_DECLS
#endif
