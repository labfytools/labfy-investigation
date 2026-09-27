#include "database/database.h"
#include "database/schema.h"
#include "database/statement.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>
static char *temp_path(void){char*p=NULL;int fd=g_file_open_tmp("labfy-specimen-v21-XXXXXX.sqlite",&p,NULL);g_assert_cmpint(fd,>=,0);close(fd);g_unlink(p);return p;}
static char *scalar(Database*d,const char*sql){DatabaseStatement*s=database_statement_prepare(d,sql);g_assert_nonnull(s);g_assert_cmpint(database_statement_step(s),==,DATABASE_STATEMENT_STEP_ROW);char*v=NULL;g_assert_true(database_statement_column_text(s,0,&v));database_statement_finalize(s);return v;}
static void test_direct_install_reopen(void){char*p=temp_path();Database*d=database_open(p);g_assert_nonnull(d);g_assert_true(schema_install_v21_direct(d));char*v=scalar(d,"SELECT value FROM metadata WHERE key='schema_version';");g_assert_cmpstr(v,==,"21");g_free(v);v=scalar(d,"SELECT count(*) FROM sqlite_schema WHERE type='table' AND name IN('bank_observations','bank_proposals','bank_observation_revisions','bank_decisions','bank_accounts','declared_bank_holders','financial_transactions');");g_assert_cmpstr(v,==,"7");g_free(v);v=scalar(d,"PRAGMA foreign_keys;");g_assert_cmpstr(v,==,"1");g_free(v);database_close(d);d=database_open(p);v=scalar(d,"SELECT value FROM metadata WHERE key='schema_version';");g_assert_cmpstr(v,==,"21");g_free(v);database_close(d);g_unlink(p);g_free(p);}
static void test_direct_refuses_nonempty(void){char*p=temp_path();Database*d=database_open(p);g_assert_true(schema_install_v21_direct(d));g_assert_false(schema_install_v21_direct(d));char*v=scalar(d,"SELECT value FROM metadata WHERE key='schema_version';");g_assert_cmpstr(v,==,"21");g_free(v);database_close(d);g_unlink(p);g_free(p);}
int main(int argc,char**argv){g_test_init(&argc,&argv,NULL);g_test_add_func("/schema-v21/direct-reopen",test_direct_install_reopen);g_test_add_func("/schema-v21/refuse-nonempty",test_direct_refuses_nonempty);return g_test_run();}
