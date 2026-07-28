#ifndef LABFY_INVESTIGATION_PERSON_CREATION_GUARD_H
#define LABFY_INVESTIGATION_PERSON_CREATION_GUARD_H
#include <glib.h>
G_BEGIN_DECLS
typedef struct PersonCreationGuard PersonCreationGuard;
PersonCreationGuard *person_creation_guard_new(guint64 generation,
    const char *root_path, const char *database_path);
void person_creation_guard_free(PersonCreationGuard *guard);
gboolean person_creation_guard_matches(const PersonCreationGuard *guard,
    gboolean session_is_open, guint64 generation, const char *root_path,
    const char *database_path);
G_END_DECLS
#endif
