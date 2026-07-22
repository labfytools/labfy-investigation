#include "models/osint_action_catalog.h"

#include <glib.h>

static OsintSelectionContext *create_entity_context(const char *type)
{
    EntityRecord *entity = entity_record_new(
        "11111111-1111-4111-8111-111111111111", type, "example.org",
        NULL, NULL, 80, "2026-01-01T00:00:00Z",
        "2026-01-01T00:00:00Z", ENTITY_STATUS_ACTIVE, NULL
    );
    OsintSelectionContext *context =
        osint_selection_context_new_entity(entity, NULL);
    entity_record_free(entity);
    return context;
}

static void test_domain_actions(void)
{
    OsintActionCatalog *catalog = osint_action_catalog_new_defaults();
    OsintSelectionContext *context = create_entity_context("domain");
    GPtrArray *actions = osint_action_catalog_list_compatible(catalog, context);

    g_assert_nonnull(actions);
    g_assert_cmpuint(actions->len, ==, 2U);
    g_assert_cmpstr(osint_action_get_identifier(g_ptr_array_index(actions, 0)),
        ==, "selection-preview");
    g_assert_true(osint_action_is_available(g_ptr_array_index(actions, 0)));
    g_assert_false(osint_action_is_available(g_ptr_array_index(actions, 1)));
    g_assert_cmpstr(osint_action_get_required_tool_identifier(
        g_ptr_array_index(actions, 1)), ==, "dig");
    g_assert_nonnull(osint_action_get_unavailable_reason(
        g_ptr_array_index(actions, 1)));

    g_ptr_array_unref(actions);
    osint_selection_context_free(context);
    osint_action_catalog_free(catalog);
}

static void test_non_domain_actions(void)
{
    OsintActionCatalog *catalog = osint_action_catalog_new_defaults();
    OsintSelectionContext *context = create_entity_context("person");
    GPtrArray *actions = osint_action_catalog_list_compatible(catalog, context);
    g_assert_cmpuint(actions->len, ==, 1U);
    g_assert_cmpstr(osint_action_get_identifier(g_ptr_array_index(actions, 0)),
        ==, "selection-preview");
    g_ptr_array_unref(actions);
    osint_selection_context_free(context);
    osint_action_catalog_free(catalog);
}

static void test_invalid_arguments(void)
{
    OsintActionCatalog *catalog = osint_action_catalog_new_defaults();
    g_assert_null(osint_action_catalog_list_compatible(catalog, NULL));
    g_assert_null(osint_action_catalog_list_compatible(NULL, NULL));
    osint_action_catalog_free(catalog);
    osint_action_catalog_free(NULL);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/osint-action-catalog/domain", test_domain_actions);
    g_test_add_func("/osint-action-catalog/non-domain", test_non_domain_actions);
    g_test_add_func("/osint-action-catalog/invalid", test_invalid_arguments);
    return g_test_run();
}
