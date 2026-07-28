#include "core/person_creation_guard.h"
struct PersonCreationGuard {
    guint64 generation;
    char *root_path;
    char *database_path;
};
PersonCreationGuard *person_creation_guard_new(guint64 generation,
    const char *root_path, const char *database_path)
{
    if (root_path == NULL || database_path == NULL) return NULL;
    PersonCreationGuard *g = g_new0(PersonCreationGuard, 1);
    g->generation = generation;
    g->root_path = g_canonicalize_filename(root_path, NULL);
    g->database_path = g_canonicalize_filename(database_path, NULL);
    return g;
}
void person_creation_guard_free(PersonCreationGuard *g)
{
    if (g == NULL) return;
    g_free(g->root_path); g_free(g->database_path); g_free(g);
}
gboolean person_creation_guard_matches(const PersonCreationGuard *g,
    gboolean open, guint64 generation, const char *root, const char *database)
{
    char *canonical_root, *canonical_database;
    gboolean matches;
    if (g == NULL || !open || root == NULL || database == NULL ||
        generation != g->generation) return FALSE;
    canonical_root = g_canonicalize_filename(root, NULL);
    canonical_database = g_canonicalize_filename(database, NULL);
    matches = g_strcmp0(canonical_root, g->root_path) == 0 &&
        g_strcmp0(canonical_database, g->database_path) == 0;
    g_free(canonical_root); g_free(canonical_database);
    return matches;
}
