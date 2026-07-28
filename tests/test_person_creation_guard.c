#include "core/person_creation_guard.h"
#include <glib.h>
static void test_guard(void)
{
    PersonCreationGuard *guard = person_creation_guard_new(3,
        "/tmp/synthetic-a", "/tmp/synthetic-a/Enquete.sqlite");
    g_assert_true(person_creation_guard_matches(guard, TRUE, 3,
        "/tmp/synthetic-a", "/tmp/synthetic-a/Enquete.sqlite"));
    g_assert_false(person_creation_guard_matches(guard, TRUE, 4,
        "/tmp/synthetic-a", "/tmp/synthetic-a/Enquete.sqlite"));
    g_assert_false(person_creation_guard_matches(guard, TRUE, 3,
        "/tmp/synthetic-a", "/tmp/synthetic-b/Enquete.sqlite"));
    g_assert_false(person_creation_guard_matches(guard, FALSE, 3,
        "/tmp/synthetic-a", "/tmp/synthetic-a/Enquete.sqlite"));
    g_assert_false(person_creation_guard_matches(guard, TRUE, 3,
        NULL, NULL));
    person_creation_guard_free(guard);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-creation-guard/session", test_guard);
    return g_test_run();
}
