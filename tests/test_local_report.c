#include "core/local_report_service.h"
#include "core/core_graph_specimen.h"
#include "database/transaction.h"
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <sqlite3.h>

typedef struct { char *directory; CoreGraphSpecimenPaths paths; Database *database; char *investigation_id; } Fixture;

static void remove_tree(const char *path) {
  GDir *directory=g_dir_open(path,0,NULL);const char *name=NULL;
  if(directory==NULL){g_remove(path);return;}
  while((name=g_dir_read_name(directory))!=NULL){char *child=g_build_filename(path,name,NULL);if(g_file_test(child,G_FILE_TEST_IS_DIR))remove_tree(child);else g_remove(child);g_free(child);}g_dir_close(directory);g_rmdir(path);
}

static void setup(Fixture *fixture,gconstpointer unused) {
  (void)unused;GError *error=NULL;fixture->directory=g_dir_make_tmp("labfy-report-XXXXXX",&error);g_assert_no_error(error);
  g_assert_true(core_graph_specimen_generate(fixture->directory,&fixture->paths,&error));g_assert_no_error(error);
  fixture->database=database_open_read_only(fixture->paths.database_path,&error);g_assert_no_error(error);
  JsonParser *parser=json_parser_new();g_assert_true(json_parser_load_from_file(parser,fixture->paths.snapshot_path,&error));
  JsonObject *root=json_node_get_object(json_parser_get_root(parser));
  fixture->investigation_id=g_strdup(json_object_get_string_member(
      json_object_get_object_member(root,"investigation"),"id"));g_object_unref(parser);
}

static void teardown(Fixture *fixture,gconstpointer unused) {(void)unused;database_close(fixture->database);g_free(fixture->investigation_id);core_graph_specimen_paths_clear(&fixture->paths);remove_tree(fixture->directory);g_free(fixture->directory);}

static GBytes *build(Fixture *fixture,const char *const *ids,gsize count,LocalReportLimits limits,GError **error) {
  LocalReportRequest request={"Rapport SPECIMEN","Commentaire humain.","MINIMAL","2026-09-26T22:00:00Z",ids,count,TRUE,TRUE,TRUE};
  return local_report_service_build(fixture->database,fixture->investigation_id,&request,limits,error);
}

static void test_selection_and_transaction(Fixture *fixture,gconstpointer unused) {
  (void)unused;const char *ids[]={"evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_A};GError *error=NULL;
  GBytes *first=build(fixture,ids,1,(LocalReportLimits){4U,16U,16U,1024U*1024U},&error);g_assert_no_error(error);g_assert_nonnull(first);
  g_assert_true(database_transaction_begin_read_only(fixture->database));GBytes *borrowed=build(fixture,ids,1,(LocalReportLimits){4U,16U,16U,1024U*1024U},&error);g_assert_nonnull(borrowed);g_assert_true(database_transaction_is_active(fixture->database));g_assert_true(database_transaction_rollback(fixture->database));
  g_assert_true(g_bytes_equal(first,borrowed));g_bytes_unref(borrowed);g_bytes_unref(first);
}

static void test_temporal_projection_and_revision(Fixture *fixture,gconstpointer unused) {
  (void)unused;GError *error=NULL;const char *ids[]={"evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_A};
  LocalReportLimits limits={4U,16U,32U,1024U*1024U};
  LocalReportRequest full={"Rapport SPECIMEN","Humain","MINIMAL","2026-09-26T22:00:00Z",ids,1,TRUE,TRUE,TRUE};
  GBytes *a=local_report_service_build(fixture->database,fixture->investigation_id,&full,limits,&error);g_assert_no_error(error);g_assert_nonnull(a);
  gsize size=0;const char *json=g_bytes_get_data(a,&size);g_assert_nonnull(g_strstr_len(json,size,"evidence_import"));
  full.include_timeline=FALSE;GBytes *b=local_report_service_build(fixture->database,fixture->investigation_id,&full,limits,&error);g_assert_no_error(error);g_assert_nonnull(b);g_assert_false(g_bytes_equal(a,b));
  g_bytes_unref(b);g_bytes_unref(a);
}

static void test_minute_temporal_projection(Fixture *fixture,gconstpointer unused) {
  (void)unused;
  database_close(fixture->database);
  fixture->database=NULL;
  sqlite3 *sqlite=NULL;
  g_assert_cmpint(sqlite3_open(fixture->paths.database_path,&sqlite),==,SQLITE_OK);
  const char *sql=
      "UPDATE preuves SET imported_at='2026-09-26T14:05+02:00', "
      "collected_at='2026-09-26T12:05Z' WHERE id='" CORE_GRAPH_SPECIMEN_EVIDENCE_A "';"
      "UPDATE preuves SET imported_at='2026-09-26T14:05-03:30' WHERE id='" CORE_GRAPH_SPECIMEN_EVIDENCE_B "';";
  g_assert_cmpint(sqlite3_exec(sqlite,sql,NULL,NULL,NULL),==,SQLITE_OK);
  g_assert_cmpint(sqlite3_close(sqlite),==,SQLITE_OK);
  GError *error=NULL;
  fixture->database=database_open_read_only(fixture->paths.database_path,&error);
  g_assert_no_error(error);

  const char *ids[]={"evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_A,
                     "evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_B};
  GBytes *bytes=build(fixture,ids,2,
      (LocalReportLimits){4U,16U,32U,1024U*1024U},&error);
  g_assert_no_error(error);
  g_assert_nonnull(bytes);
  gsize size=0U;
  const char *json=g_bytes_get_data(bytes,&size);
  g_assert_nonnull(g_strstr_len(json,size,
      "\"raw_value\":\"2026-09-26T14:05+02:00\""));
  g_assert_nonnull(g_strstr_len(json,size,"\"precision\":\"minute\""));
  g_assert_nonnull(g_strstr_len(json,size,"\"timezone\":\"UTC\""));
  g_assert_nonnull(g_strstr_len(json,size,"\"timezone\":\"explicit_offset\""));
  g_assert_nonnull(g_strstr_len(json,size,"\"positionable\":true"));
  g_bytes_unref(bytes);
}

static void test_invalid_and_limits(Fixture *fixture,gconstpointer unused) {
  (void)unused;GError *error=NULL;const char *unknown[]={"evidence:99000000-0000-4000-8000-000000000001"};
  g_assert_null(build(fixture,unknown,1,(LocalReportLimits){1U,1U,1U,4096U},&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND);g_clear_error(&error);
  const char *ids[]={"evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_A,"evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_B};
  g_assert_null(build(fixture,ids,2,(LocalReportLimits){1U,16U,16U,4096U},&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT);g_clear_error(&error);
  g_assert_null(build(fixture,ids,2,(LocalReportLimits){2U,16U,16U,32U},&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE);g_clear_error(&error);
  LocalReportRequest empty={"Rapport",NULL,"MINIMAL","2026-09-26T22:00:00Z",NULL,0,TRUE,TRUE,TRUE};
  g_assert_null(local_report_service_build(fixture->database,fixture->investigation_id,&empty,(LocalReportLimits){2U,2U,2U,4096U},NULL));
  const char *dated[]={"evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_A};
  g_assert_null(build(fixture,dated,1,(LocalReportLimits){2U,16U,1U,65536U},&error));
  g_assert_error(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE);g_clear_error(&error);
}

static void test_sections_are_effective(Fixture *fixture,gconstpointer unused) {
  (void)unused;GError *error=NULL;const char *ids[]={"evidence:" CORE_GRAPH_SPECIMEN_EVIDENCE_A};
  LocalReportRequest request={"Rapport","Humain","MINIMAL","2026-09-26T22:00:00Z",ids,1,FALSE,FALSE,FALSE};
  GBytes *bytes=local_report_service_build(fixture->database,fixture->investigation_id,&request,(LocalReportLimits){2U,16U,16U,65536U},&error);g_assert_no_error(error);g_assert_nonnull(bytes);
  gsize size=0;const char *data=g_bytes_get_data(bytes,&size);JsonParser *parser=json_parser_new();g_assert_true(json_parser_load_from_data(parser,data,size,&error));g_assert_no_error(error);
  JsonObject *root=json_node_get_object(json_parser_get_root(parser));g_assert_cmpuint(json_array_get_length(json_object_get_array_member(root,"network")),==,0U);g_assert_cmpuint(json_array_get_length(json_object_get_array_member(root,"timeline")),==,0U);
  JsonObject *object=json_array_get_object_element(json_object_get_array_member(root,"objects"),0U);g_assert_true(json_object_get_boolean_member(object,"reference_only"));g_assert_true(JSON_NODE_HOLDS_NULL(json_object_get_member(object,"label")));
  g_object_unref(parser);g_bytes_unref(bytes);
}

int main(int argc,char **argv){g_test_init(&argc,&argv,NULL);g_test_add("/local-report/selection-transaction",Fixture,NULL,setup,test_selection_and_transaction,teardown);g_test_add("/local-report/temporal-revision",Fixture,NULL,setup,test_temporal_projection_and_revision,teardown);g_test_add("/local-report/minute-temporal",Fixture,NULL,setup,test_minute_temporal_projection,teardown);g_test_add("/local-report/invalid-limits",Fixture,NULL,setup,test_invalid_and_limits,teardown);g_test_add("/local-report/sections",Fixture,NULL,setup,test_sections_are_effective,teardown);return g_test_run();}
