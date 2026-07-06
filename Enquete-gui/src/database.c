#include "database.h"
#include <stdio.h>

int db_open(sqlite3 **db, const char *path)
{
    int rc = sqlite3_open(path, db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQLite: %s\n", sqlite3_errmsg(*db));
        return 1;
    }

    return 0;
}

void db_close(sqlite3 *db)
{
    if (db != NULL) {
        sqlite3_close(db);
    }
}
