#include "core/relation_type_service.h"
#include "database/database.h"
#include <glib.h>
#include <glib/gstdio.h>

static void test_service_crud_and_merge(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-relation-types-XXXXXX", &error);
    char *path = g_build_filename(directory, "Enquete.sqlite", NULL);
    Database *database = NULL;
    RelationTypeService *service = NULL;
    gint64 email_id = 0; gint64 source_id = 0; gint64 target_id = 0;
    guint64 count = 99;

    g_assert_no_error(error);
    g_assert_true(database_initialize(path, "Types", directory));
    database = database_open(path);
    g_assert_nonnull(database);
    g_assert_true(database_migrate_to_latest(database));
    service = relation_type_service_new(database, &error);
    g_assert_no_error(error); g_assert_nonnull(service);
    g_assert_true(relation_type_service_create_custom(
        service, " Email ", NULL, &email_id, &error));
    g_assert_cmpint(email_id, >, 0);
    g_assert_false(relation_type_service_create_custom(
        service, "EMAIL", NULL, NULL, &error));
    g_assert_error(error, RELATION_TYPE_SERVICE_ERROR,
        RELATION_TYPE_SERVICE_ERROR_DUPLICATE);
    g_clear_error(&error);
    g_assert_true(relation_type_service_rename(
        service, email_id, "Adresse e-mail", &error));
    g_assert_true(relation_type_service_count_usage(
        service, email_id, &count, &error));
    g_assert_cmpuint(count, ==, 0U);
    g_assert_true(relation_type_service_delete(service, email_id, &error));
    g_assert_true(relation_type_service_create_custom(
        service, "Source temporaire", NULL, &source_id, &error));
    g_assert_true(relation_type_service_create_custom(
        service, "Cible temporaire", NULL, &target_id, &error));
    g_assert_true(relation_type_service_merge(
        service, source_id, target_id, &error));

    relation_type_service_free(service); database_close(database);
    g_remove(path); g_rmdir(directory); g_free(path); g_free(directory);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/relation-type-service/crud-merge",
        test_service_crud_and_merge);
    return g_test_run();
}
