#include "core/core_graph_specimen.h"
#include "core/local_capability_registry.h"
#include "core/local_job_store.h"
#include "core/local_planner_service.h"
#include "dao/investigation_dao.h"
#include "database/database.h"
#include "models/investigation_record.h"
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>

static void remove_tree(const char *path) {
  GDir *directory=g_dir_open(path,0,NULL);const char *name=NULL;
  if(!directory){g_remove(path);return;}while((name=g_dir_read_name(directory))){
    char *child=g_build_filename(path,name,NULL);if(g_file_test(child,G_FILE_TEST_IS_DIR))remove_tree(child);else g_remove(child);g_free(child);}
  g_dir_close(directory);g_rmdir(path);
}

static void test_planner_snapshot_is_stable(void) {
  GError *error=NULL;char *directory=g_dir_make_tmp("labfy-planner-XXXXXX",&error);
  CoreGraphSpecimenPaths paths={0};g_assert_true(core_graph_specimen_generate(directory,&paths,&error));
  Database *database=database_open(paths.database_path);InvestigationRecord *investigation=investigation_dao_load(database);
  const char *id=investigation_record_get_id(investigation);char *jobs=g_build_filename(directory,"jobs.sqlite",NULL);
  LocalJobStore *store=local_job_store_create(jobs,id,&error);LocalCapabilityRegistry *registry=local_capability_registry_new(&error);
  GBytes *first=local_planner_service_build(database,store,registry,id,1024U*1024U,&error);
  GBytes *second=local_planner_service_build(database,store,registry,id,1024U*1024U,&error);
  g_assert_no_error(error);g_assert_nonnull(first);g_assert_true(g_bytes_equal(first,second));
  gsize size=0;const char *data=g_bytes_get_data(first,&size);JsonParser *parser=json_parser_new();
  g_assert_true(json_parser_load_from_data(parser,data,(gssize)size,&error));JsonObject *root=json_node_get_object(json_parser_get_root(parser));
  g_assert_cmpstr(json_object_get_string_member(root,"contract"),==,LOCAL_PLANNER_CONTRACT);
  g_assert_true(json_object_get_boolean_member(root,"complete"));
  JsonArray *recommendations=json_object_get_array_member(root,"recommendations");
  for(guint i=0;i<json_array_get_length(recommendations);i++){
    JsonObject *item=json_array_get_object_element(recommendations,i);
    g_assert_cmpstr(json_object_get_string_member(item,"network_contact"),==,"NONE");
    g_assert_true(json_object_has_member(item,"reason_code"));
    g_assert_true(json_object_has_member(item,"input_revision"));
  }
  g_object_unref(parser);g_bytes_unref(second);g_bytes_unref(first);local_capability_registry_free(registry);
  local_job_store_close(store);investigation_record_free(investigation);database_close(database);g_free(jobs);
  core_graph_specimen_paths_clear(&paths);remove_tree(directory);g_free(directory);
}

static void test_planner_rejects_bounds(void) {
  GError *error=NULL;g_assert_null(local_planner_service_build(NULL,NULL,NULL,
      "not-an-id",0,&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT);g_clear_error(&error);
}

int main(int argc,char **argv){g_test_init(&argc,&argv,NULL);
  g_test_add_func("/local-planner/stable-snapshot",test_planner_snapshot_is_stable);
  g_test_add_func("/local-planner/invalid-bounds",test_planner_rejects_bounds);
  return g_test_run();}
