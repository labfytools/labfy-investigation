#include "models/osint_selection_context.h"

#include <glib.h>

static EntityRecord *create_entity(const char *identifier, const char *value)
{
    return entity_record_new(
        identifier, "domain", value, NULL, NULL, 80,
        "2026-01-01T00:00:00Z", "2026-01-01T00:00:00Z",
        ENTITY_STATUS_ACTIVE, NULL
    );
}

static void test_entity_context(void)
{
    EntityRecord *entity = create_entity(
        "11111111-1111-4111-8111-111111111111", "example.org"
    );
    OsintSelectionContext *context =
        osint_selection_context_new_entity(entity, NULL);

    g_assert_nonnull(context);
    g_assert_cmpint(osint_selection_context_get_kind(context), ==,
        OSINT_SELECTION_CONTEXT_KIND_ENTITY);
    g_assert_cmpstr(osint_selection_context_get_type(context), ==, "domain");
    g_assert_cmpstr(osint_selection_context_get_value(context), ==, "example.org");
    g_assert_null(osint_selection_context_get_source_value(context));

    entity_record_free(entity);
    g_assert_cmpstr(osint_selection_context_get_value(context), ==, "example.org");
    osint_selection_context_free(context);
}

static void test_relation_context(void)
{
    EntityRecord *source = create_entity(
        "11111111-1111-4111-8111-111111111111", "source.example"
    );
    EntityRecord *target = create_entity(
        "22222222-2222-4222-8222-222222222222", "target.example"
    );
    RelationRecord *relation = relation_record_new(
        "33333333-3333-4333-8333-333333333333",
        entity_record_get_identifier(source), entity_record_get_identifier(target),
        "redirects_to", "Redirection", NULL, 90,
        "2026-01-01T00:00:00Z", "2026-01-01T00:00:00Z",
        RELATION_STATUS_ACTIVE, NULL
    );
    OsintSelectionContext *context = osint_selection_context_new_relation(
        relation, source, target, NULL
    );

    g_assert_nonnull(context);
    g_assert_cmpint(osint_selection_context_get_kind(context), ==,
        OSINT_SELECTION_CONTEXT_KIND_RELATION);
    g_assert_cmpstr(osint_selection_context_get_value(context), ==, "Redirection");
    g_assert_cmpstr(osint_selection_context_get_source_value(context), ==,
        "source.example");
    g_assert_cmpstr(osint_selection_context_get_target_value(context), ==,
        "target.example");

    osint_selection_context_free(context);
    relation_record_free(relation);
    entity_record_free(target);
    entity_record_free(source);
}

static void test_invalid_context(void)
{
    GError *error = NULL;
    g_assert_null(osint_selection_context_new_entity(NULL, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error(&error);
    osint_selection_context_free(NULL);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/osint-selection-context/entity", test_entity_context);
    g_test_add_func("/osint-selection-context/relation", test_relation_context);
    g_test_add_func("/osint-selection-context/invalid", test_invalid_context);
    return g_test_run();
}
