#define _POSIX_C_SOURCE 200809L
#include "core/local_correlation_service.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "models/evidence_observation.h"
#include "models/evidence_record.h"
#include "database/transaction.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    EvidenceObservation *observation;
    EvidenceRecord *evidence;
    char *normalized;
    char *reason;
    gboolean eligible;
} IndexedObservation;

static void indexed_observation_free(IndexedObservation *item)
{
    if (item == NULL) return;
    evidence_observation_free(item->observation);
    g_free(item->normalized);
    g_free(item->reason);
    g_free(item);
}

static gboolean supported_type(const char *type)
{
    return g_strcmp0(type, "email_address") == 0 ||
        g_strcmp0(type, "domain_name") == 0 ||
        g_strcmp0(type, "ip_address") == 0;
}

static gboolean ascii_domain_valid(const char *value)
{
    if (value == NULL || value[0] == '\0' || strlen(value) > 253U ||
        value[0] == '.' || value[strlen(value) - 1U] == '.') return FALSE;
    gboolean dot = FALSE;
    gsize label_length = 0U;
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor != '\0'; cursor++) {
        if (*cursor == '.') {
            if (label_length == 0U || label_length > 63U || cursor[-1] == '-')
                return FALSE;
            dot = TRUE; label_length = 0U; continue;
        }
        if (!(g_ascii_isalnum(*cursor) ||
              (*cursor == '-' && label_length > 0U))) return FALSE;
        label_length++;
    }
    return dot && label_length > 0U && label_length <= 63U &&
        value[strlen(value) - 1U] != '-';
}

char *local_correlation_normalize(const char *type, const char *raw,
    char **out_reason)
{
    g_return_val_if_fail(out_reason != NULL, NULL);
    *out_reason = NULL;
    if (raw == NULL || raw[0] == '\0') {
        *out_reason = g_strdup("Valeur absente."); return NULL;
    }
    if (g_strcmp0(type, "email_address") == 0) {
        const char *at = strchr(raw, '@');
        if (at == NULL || at == raw || at[1] == '\0' ||
            strchr(at + 1, '@') != NULL || (gsize)(at - raw) > 64U ||
            !g_utf8_validate(raw, -1, NULL) || !ascii_domain_valid(at + 1)) {
            *out_reason = g_strdup("Adresse e-mail complexe ou invalide non rapprochée.");
            return NULL;
        }
        char *domain = g_ascii_strdown(at + 1, -1);
        char *result = g_strdup_printf("%.*s@%s", (int)(at - raw), raw, domain);
        g_free(domain);
        return result;
    }
    if (g_strcmp0(type, "domain_name") == 0) {
        char *candidate = g_strdup(raw);
        if (candidate[strlen(candidate) - 1U] == '.')
            candidate[strlen(candidate) - 1U] = '\0';
        if (!ascii_domain_valid(candidate)) {
            g_free(candidate);
            *out_reason = g_strdup("Nom de domaine non ASCII ou invalide non rapproché.");
            return NULL;
        }
        char *result = g_ascii_strdown(candidate, -1);
        g_free(candidate);
        return result;
    }
    if (g_strcmp0(type, "ip_address") == 0) {
        GInetAddress *address = g_inet_address_new_from_string(raw);
        if (address == NULL) {
            *out_reason = g_strdup("Adresse IP invalide non rapprochée."); return NULL;
        }
        char *result = g_inet_address_to_string(address);
        g_object_unref(address);
        return result;
    }
    *out_reason = g_strdup("Type non indexé par ce lot.");
    return NULL;
}

static char *normalize_observation(const EvidenceObservation *observation,
    char **out_reason)
{
    const char *raw = observation->value_raw != NULL
        ? observation->value_raw : observation->value;
    return local_correlation_normalize(observation->type_identifier, raw,
        out_reason);
}

static void json_string(JsonBuilder *builder, const char *name, const char *value)
{
    json_builder_set_member_name(builder, name);
    if (value != NULL) json_builder_add_string_value(builder, value);
    else json_builder_add_null_value(builder);
}

static char *stable_id(const char *prefix, const char *input)
{
    char *hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256, input, -1);
    char *result = g_strdup_printf("%s:%s", prefix, hash);
    g_free(hash);
    return result;
}

static char *group_identifier(const char *key, GPtrArray *members,
    const char *revision)
{
    GString *identity = g_string_new(key);
    for (guint i = 0; i < members->len; i++) {
        IndexedObservation *entry = g_ptr_array_index(members, i);
        g_string_append_c(identity, '\x1e');
        g_string_append(identity, entry->observation->identifier);
    }
    g_string_append_c(identity, '\x1d'); g_string_append(identity, revision);
    char *result = stable_id("correlation", identity->str);
    g_string_free(identity, TRUE); return result;
}

static gboolean groups_share_evidence(GPtrArray *left, GPtrArray *right,
    const char **out_evidence)
{
    for (guint i = 0; i < left->len; i++) {
        const char *candidate = ((IndexedObservation *)g_ptr_array_index(
            left, i))->observation->evidence_identifier;
        for (guint j = 0; j < right->len; j++)
            if (g_strcmp0(candidate, ((IndexedObservation *)g_ptr_array_index(
                    right, j))->observation->evidence_identifier) == 0) {
                *out_evidence = candidate; return TRUE;
            }
    }
    return FALSE;
}

static gint indexed_compare(gconstpointer left, gconstpointer right)
{
    const IndexedObservation *a = *(IndexedObservation * const *)left;
    const IndexedObservation *b = *(IndexedObservation * const *)right;
    return g_strcmp0(a->observation->identifier, b->observation->identifier);
}

/* CONTRACT: chaque champ est encodé par longueur puis octets. Cette forme
 * interdit les collisions qu'une concaténation avec séparateur autoriserait. */
static void checksum_field(GChecksum *checksum, const char *value)
{
    guint64 length = value != NULL ? strlen(value) : G_MAXUINT64;
    guint8 encoded[8];
    for (guint i = 0; i < 8U; i++)
        encoded[i] = (guint8)(length >> ((7U - i) * 8U));
    g_checksum_update(checksum, encoded, sizeof(encoded));
    if (value != NULL) g_checksum_update(checksum,
        (const guchar *)value, (gssize)length);
}

static void checksum_limit(GChecksum *checksum, guint64 value)
{
    guint8 encoded[8];
    for (guint i = 0; i < 8U; i++)
        encoded[i] = (guint8)(value >> ((7U - i) * 8U));
    g_checksum_update(checksum, encoded, sizeof(encoded));
}

GBytes *local_correlation_service_build(Database *database,
    const char *investigation_identifier, LocalCorrelationLimits limits,
    GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (database == NULL || !g_uuid_string_is_valid(investigation_identifier) ||
        limits.max_observations == 0U || limits.max_groups == 0U ||
        limits.max_connections == 0U ||
        limits.max_json_bytes == 0U) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Contrat de corrélation locale invalide."); return NULL;
    }
    gboolean owns_transaction = !database_transaction_is_active(database);
    if (owns_transaction && !database_transaction_begin_read_only(database)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "Impossible d'ouvrir le snapshot SQLite de corrélation.");
        return NULL;
    }
    EvidenceDao *evidence_dao = evidence_dao_new(database, error);
    EvidenceEntityDao *observation_dao = evidence_dao != NULL
        ? evidence_entity_dao_new(database, error) : NULL;
    GPtrArray *evidence = observation_dao != NULL
        ? evidence_dao_list_all(evidence_dao, error) : NULL;
    GPtrArray *indexed = evidence != NULL
        ? g_ptr_array_new_with_free_func((GDestroyNotify)indexed_observation_free)
        : NULL;
    gboolean truncated = FALSE;
    for (guint i = 0; indexed != NULL && i < evidence->len; i++) {
        EvidenceRecord *record = g_ptr_array_index(evidence, i);
        GPtrArray *items = evidence_entity_dao_list_observations(observation_dao,
            evidence_record_get_identifier(record), error);
        if (items == NULL) { g_clear_pointer(&indexed, g_ptr_array_unref); break; }
        while (items->len > 0U) {
            EvidenceObservation *observation = g_ptr_array_steal_index(items, 0U);
            if (!supported_type(observation->type_identifier)) {
                evidence_observation_free(observation); continue;
            }
            if (indexed->len >= limits.max_observations) {
                truncated = TRUE; evidence_observation_free(observation); continue;
            }
            IndexedObservation *entry = g_new0(IndexedObservation, 1);
            entry->observation = observation;
            entry->evidence = record;
            entry->normalized = normalize_observation(observation, &entry->reason);
            entry->eligible = entry->normalized != NULL &&
                g_strcmp0(observation->verification_status, "rejected") != 0 &&
                g_strcmp0(observation->verification_status, "invalid") != 0;
            if (!entry->eligible && entry->reason == NULL)
                entry->reason = g_strdup("Observation rejetée ou invalide.");
            g_ptr_array_add(indexed, entry);
        }
        g_ptr_array_unref(items);
    }
    if (indexed == NULL) goto failure;
    g_ptr_array_sort(indexed, indexed_compare);
    GChecksum *revision_checksum = g_checksum_new(G_CHECKSUM_SHA256);
    checksum_field(revision_checksum, LOCAL_CORRELATION_CONTRACT);
    checksum_field(revision_checksum, LOCAL_CORRELATION_RULE_VERSION);
    checksum_field(revision_checksum, investigation_identifier);
    /* INVARIANT: une projection calculée avec une autre enveloppe ne peut pas
     * réutiliser la même révision, même si le préfixe retenu coïncide. */
    checksum_limit(revision_checksum, limits.max_observations);
    checksum_limit(revision_checksum, limits.max_groups);
    checksum_limit(revision_checksum, limits.max_connections);
    checksum_limit(revision_checksum, limits.max_json_bytes);
    GHashTable *groups = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
        (GDestroyNotify)g_ptr_array_unref);
    for (guint i = 0; i < indexed->len; i++) {
        IndexedObservation *entry = g_ptr_array_index(indexed, i);
        EvidenceObservation *item = entry->observation;
        checksum_field(revision_checksum, item->identifier);
        checksum_field(revision_checksum, item->type_identifier);
        checksum_field(revision_checksum, item->value);
        checksum_field(revision_checksum, item->value_raw);
        checksum_field(revision_checksum, item->value_normalized);
        checksum_field(revision_checksum, item->value_corrected);
        checksum_field(revision_checksum, item->verification_status);
        checksum_field(revision_checksum, item->provenance_kind);
        checksum_field(revision_checksum, item->source_header);
        checksum_field(revision_checksum, item->extraction_identifier);
        checksum_field(revision_checksum, item->evidence_identifier);
        checksum_field(revision_checksum, entry->normalized);
        checksum_field(revision_checksum, evidence_record_get_sha256(entry->evidence));
        if (!entry->eligible) continue;
        char *key = g_strdup_printf("%s\x1f%s", entry->observation->type_identifier,
            entry->normalized);
        GPtrArray *members = g_hash_table_lookup(groups, key);
        if (members == NULL) {
            members = g_ptr_array_new(); g_hash_table_insert(groups, key, members);
        } else g_free(key);
        g_ptr_array_add(members, entry);
    }
    const char *revision = g_checksum_get_string(revision_checksum);
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_string(builder, "contract", LOCAL_CORRELATION_CONTRACT);
    json_string(builder, "investigation_id", investigation_identifier);
    json_string(builder, "revision", revision);
    json_string(builder, "rule_version", LOCAL_CORRELATION_RULE_VERSION);
    json_builder_set_member_name(builder, "observations");
    json_builder_begin_array(builder);
    for (guint i = 0; i < indexed->len; i++) {
        IndexedObservation *entry = g_ptr_array_index(indexed, i);
        EvidenceObservation *item = entry->observation;
        json_builder_begin_object(builder);
        json_string(builder, "id", item->identifier);
        json_string(builder, "type", item->type_identifier);
        json_string(builder, "raw", item->value_raw);
        json_string(builder, "normalized", entry->normalized);
        json_string(builder, "normalization_reason", entry->eligible
            ? "Normalisation typée prudente; aucune attribution d'identité."
            : entry->reason);
        json_string(builder, "review_state", item->verification_status);
        json_string(builder, "evidence_id", item->evidence_identifier);
        json_string(builder, "extraction_id", item->extraction_identifier);
        json_string(builder, "source_header", item->source_header);
        json_string(builder, "evidence_sha256",
            evidence_record_get_sha256(entry->evidence));
        json_builder_set_member_name(builder, "eligible");
        json_builder_add_boolean_value(builder, entry->eligible);
        json_builder_set_member_name(builder, "capabilities");
        json_builder_begin_array(builder);
        if (entry->eligible) {
            json_builder_begin_object(builder);
            json_string(builder, "id", "local_occurrences");
            json_string(builder, "label", "Voir toutes les occurrences dans l'enquête");
            json_string(builder, "result_type", "occurrence_list");
            json_string(builder, "network_contact", "none");
            json_builder_end_object(builder);
            json_builder_begin_object(builder);
            json_string(builder, "id", "containing_evidence");
            json_string(builder, "label", "Voir les preuves qui contiennent cet identifiant");
            json_string(builder, "result_type", "evidence_list");
            json_string(builder, "network_contact", "none");
            json_builder_end_object(builder);
            if (g_strcmp0(item->type_identifier, "email_address") == 0) {
                json_builder_begin_object(builder);
                json_string(builder, "id", "email_domain");
                json_string(builder, "label", "Explorer le domaine de cette adresse");
                json_string(builder, "result_type", "typed_local_pivot");
                json_string(builder, "network_contact", "none");
                json_builder_end_object(builder);
            }
        }
        json_builder_end_array(builder);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_set_member_name(builder, "groups");
    json_builder_begin_array(builder);
    GList *keys = g_hash_table_get_keys(groups);
    keys = g_list_sort(keys, (GCompareFunc)g_strcmp0);
    guint group_count = 0U;
    GHashTable *exported_groups = g_hash_table_new(g_str_hash, g_str_equal);
    for (GList *cursor = keys; cursor != NULL; cursor = cursor->next) {
        GPtrArray *members = g_hash_table_lookup(groups, cursor->data);
        if (members->len < 2U) continue;
        if (group_count >= limits.max_groups) { truncated = TRUE; break; }
        group_count++;
        g_hash_table_add(exported_groups, cursor->data);
        GHashTable *proofs = g_hash_table_new(g_str_hash, g_str_equal);
        GHashTable *contents = g_hash_table_new(g_str_hash, g_str_equal);
        GHashTable *analyses = g_hash_table_new(g_str_hash, g_str_equal);
        for (guint i = 0; i < members->len; i++) {
            IndexedObservation *entry = g_ptr_array_index(members, i);
            g_hash_table_add(proofs, entry->observation->evidence_identifier);
            const char *sha = evidence_record_get_sha256(entry->evidence);
            if (sha != NULL) g_hash_table_add(contents, (gpointer)sha);
            if (entry->observation->extraction_identifier != NULL)
                g_hash_table_add(analyses, entry->observation->extraction_identifier);
        }
        char *identifier = group_identifier(cursor->data, members, revision);
        IndexedObservation *first = g_ptr_array_index(members, 0);
        json_builder_begin_object(builder);
        json_string(builder, "id", identifier);
        json_string(builder, "type", first->observation->type_identifier);
        json_string(builder, "normalized", first->normalized);
        json_string(builder, "rule", "same_typed_value");
        json_string(builder, "rule_version", LOCAL_CORRELATION_RULE_VERSION);
        json_string(builder, "warning",
            "Un identifiant commun, un domaine ou une IP ne confirme pas l'identité d'une personne.");
        json_builder_set_member_name(builder, "occurrence_count");
        json_builder_add_int_value(builder, members->len);
        json_builder_set_member_name(builder, "analysis_count");
        json_builder_add_int_value(builder, g_hash_table_size(analyses));
        json_builder_set_member_name(builder, "distinct_evidence_count");
        json_builder_add_int_value(builder, g_hash_table_size(proofs));
        json_builder_set_member_name(builder, "distinct_content_count");
        json_builder_add_int_value(builder, g_hash_table_size(contents));
        json_builder_set_member_name(builder, "members");
        json_builder_begin_array(builder);
        for (guint i = 0; i < members->len; i++)
            json_builder_add_string_value(builder,
                ((IndexedObservation *)g_ptr_array_index(members, i))->observation->identifier);
        json_builder_end_array(builder); json_builder_end_object(builder);
        g_free(identifier);
        g_hash_table_unref(proofs); g_hash_table_unref(contents);
        g_hash_table_unref(analyses);
    }
    g_list_free(keys);
    json_builder_end_array(builder);
    /* CONTRACT: connexion exploratoire non dirigée, bornée à deux groupes
     * partageant une preuve persistée; elle ne crée aucune relation métier. */
    json_builder_set_member_name(builder, "connections");
    json_builder_begin_array(builder);
    keys = g_hash_table_get_keys(groups);
    keys = g_list_sort(keys, (GCompareFunc)g_strcmp0);
    guint connection_count = 0U;
    gboolean more_connections = FALSE;
    for (GList *left = keys; left != NULL;
         left = left->next) {
        if (!g_hash_table_contains(exported_groups, left->data)) continue;
        GPtrArray *left_members = g_hash_table_lookup(groups, left->data);
        if (left_members->len < 2U) continue;
        for (GList *right = left->next; right != NULL;
             right = right->next) {
            if (!g_hash_table_contains(exported_groups, right->data)) continue;
            GPtrArray *right_members = g_hash_table_lookup(groups, right->data);
            const char *via = NULL;
            if (right_members->len < 2U ||
                !groups_share_evidence(left_members, right_members, &via)) continue;
            if (connection_count >= limits.max_connections) {
                more_connections = TRUE; continue;
            }
            char *source = group_identifier(left->data, left_members, revision);
            char *target = group_identifier(right->data, right_members, revision);
            json_builder_begin_object(builder);
            json_string(builder, "source", source);
            json_string(builder, "target", target);
            json_string(builder, "via_evidence_id", via);
            json_string(builder, "direction", "undirected_exploratory");
            json_builder_set_member_name(builder, "depth");
            json_builder_add_int_value(builder, 2);
            json_builder_end_object(builder);
            g_free(source); g_free(target); connection_count++;
        }
    }
    if (more_connections) truncated = TRUE;
    g_list_free(keys); json_builder_end_array(builder);
    json_builder_set_member_name(builder, "complete");
    json_builder_add_boolean_value(builder, !truncated);
    json_builder_set_member_name(builder, "limits");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "max_observations");
    json_builder_add_int_value(builder, limits.max_observations);
    json_builder_set_member_name(builder, "max_groups");
    json_builder_add_int_value(builder, limits.max_groups);
    json_builder_set_member_name(builder, "max_connections");
    json_builder_add_int_value(builder, limits.max_connections);
    json_builder_set_member_name(builder, "truncated");
    json_builder_add_boolean_value(builder, truncated);
    json_builder_end_object(builder); json_builder_end_object(builder);
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gsize size = 0U; char *data = json_generator_to_data(generator, &size);
    GBytes *result = NULL;
    if (data == NULL || size > limits.max_json_bytes)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
            "Le snapshot de corrélation dépasse la borne autorisée.");
    else result = g_bytes_new_take(data, size);
    if (result == NULL) g_free(data);
    json_node_free(root); g_object_unref(generator); g_object_unref(builder);
    g_checksum_free(revision_checksum); g_hash_table_unref(groups);
    g_hash_table_unref(exported_groups);
    g_ptr_array_unref(indexed);
    g_ptr_array_unref(evidence); evidence_entity_dao_free(observation_dao);
    evidence_dao_free(evidence_dao);
    if (owns_transaction && !database_transaction_commit(database)) {
        g_clear_pointer(&result, g_bytes_unref);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "Impossible de fermer le snapshot SQLite de corrélation.");
    }
    return result;
failure:
    if (evidence != NULL) g_ptr_array_unref(evidence);
    evidence_entity_dao_free(observation_dao); evidence_dao_free(evidence_dao);
    if (owns_transaction) (void)database_transaction_rollback(database);
    return NULL;
}

gboolean local_correlation_snapshot_write_atomic(GBytes *snapshot,
    const char *path, GError **error)
{
    if (snapshot == NULL || path == NULL || path[0] == '\0') {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Destination de snapshot invalide."); return FALSE;
    }
    char *template = g_strdup_printf("%s.XXXXXX", path);
    int fd = g_mkstemp_full(template, O_WRONLY | O_CLOEXEC, 0600);
    gsize size = 0U; const guint8 *data = g_bytes_get_data(snapshot, &size);
    gsize written = 0U; gboolean ok = fd >= 0;
    while (ok && written < size) {
        ssize_t amount = write(fd, data + written, size - written);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) ok = FALSE; else written += (gsize)amount;
    }
    if (ok) ok = fsync(fd) == 0;
    if (fd >= 0 && close(fd) != 0) ok = FALSE;
    if (ok) ok = g_rename(template, path) == 0;
    if (!ok) {
        int saved = errno; (void)g_remove(template);
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(saved),
            "Publication atomique du snapshot impossible: %s", g_strerror(saved));
    }
    g_free(template); return ok;
}
