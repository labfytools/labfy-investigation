#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>

int db_open(sqlite3 **db, const char *path);
void db_close(sqlite3 *db);

#endif
