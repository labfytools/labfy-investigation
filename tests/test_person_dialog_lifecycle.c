#include "core/person_dialog_lifecycle.h"
#include <glib.h>

typedef struct
{
    guint callback_count;
    guint person_count;
    guint role_count;
    guint attachment_count;
    guint manual_source_count;
    guint graph_refresh_count;
    guint quit_count;
    gboolean main_window_active;
} CancellationEffects;

static void cancel_once(PersonDialogLifecycle *lifecycle,
    CancellationEffects *effects)
{
    if (!person_dialog_lifecycle_cancel(lifecycle)) return;
    effects->callback_count++;
}

static void assert_no_business_effect(const CancellationEffects *effects)
{
    g_assert_cmpuint(effects->person_count, ==, 0);
    g_assert_cmpuint(effects->role_count, ==, 0);
    g_assert_cmpuint(effects->attachment_count, ==, 0);
    g_assert_cmpuint(effects->manual_source_count, ==, 0);
    g_assert_cmpuint(effects->graph_refresh_count, ==, 0);
    g_assert_cmpuint(effects->quit_count, ==, 0);
    g_assert_true(effects->main_window_active);
}

static void test_cancel_on_each_step(void)
{
    for (guint step = 0; step < 4; step++) {
        PersonDialogLifecycle *lifecycle = person_dialog_lifecycle_new();
        CancellationEffects effects = {.main_window_active = TRUE};
        cancel_once(lifecycle, &effects);
        g_assert_true(person_dialog_lifecycle_is_finished(lifecycle));
        g_assert_true(person_dialog_lifecycle_is_cancelled(lifecycle));
        g_assert_cmpuint(effects.callback_count, ==, 1);
        assert_no_business_effect(&effects);
        person_dialog_lifecycle_free(lifecycle);
    }
}

static void test_close_and_double_cancel_are_idempotent(void)
{
    PersonDialogLifecycle *lifecycle = person_dialog_lifecycle_new();
    CancellationEffects effects = {.main_window_active = TRUE};
    cancel_once(lifecycle, &effects);
    cancel_once(lifecycle, &effects);
    g_assert_false(person_dialog_lifecycle_cancel(lifecycle));
    g_assert_false(person_dialog_lifecycle_complete(lifecycle));
    g_assert_cmpuint(effects.callback_count, ==, 1);
    assert_no_business_effect(&effects);
    person_dialog_lifecycle_free(lifecycle);
}

static void test_preview_is_cancelled_and_late_result_ignored(void)
{
    PersonDialogLifecycle *lifecycle = person_dialog_lifecycle_new();
    CancellationEffects effects = {.main_window_active = TRUE};
    guint64 generation =
        person_dialog_lifecycle_begin_preview(lifecycle);
    g_assert_true(person_dialog_lifecycle_has_preview(lifecycle));
    g_assert_true(person_dialog_lifecycle_accepts_preview(
        lifecycle, generation));
    cancel_once(lifecycle, &effects);
    g_assert_false(person_dialog_lifecycle_has_preview(lifecycle));
    g_assert_false(person_dialog_lifecycle_accepts_preview(
        lifecycle, generation));
    g_assert_cmpuint(person_dialog_lifecycle_get_generation(lifecycle),
        >, generation);
    assert_no_business_effect(&effects);
    person_dialog_lifecycle_free(lifecycle);
}

static void test_complete_once(void)
{
    PersonDialogLifecycle *lifecycle = person_dialog_lifecycle_new();
    g_assert_true(person_dialog_lifecycle_complete(lifecycle));
    g_assert_true(person_dialog_lifecycle_is_finished(lifecycle));
    g_assert_false(person_dialog_lifecycle_is_cancelled(lifecycle));
    g_assert_false(person_dialog_lifecycle_complete(lifecycle));
    g_assert_false(person_dialog_lifecycle_cancel(lifecycle));
    person_dialog_lifecycle_free(lifecycle);
}

static void test_null_and_full_destruction(void)
{
    g_assert_false(person_dialog_lifecycle_cancel(NULL));
    g_assert_false(person_dialog_lifecycle_complete(NULL));
    g_assert_false(person_dialog_lifecycle_accepts_preview(NULL, 1));
    person_dialog_lifecycle_free(NULL);
    for (guint i = 0; i < 128; i++) {
        PersonDialogLifecycle *lifecycle = person_dialog_lifecycle_new();
        person_dialog_lifecycle_begin_preview(lifecycle);
        person_dialog_lifecycle_cancel(lifecycle);
        person_dialog_lifecycle_free(lifecycle);
    }
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-dialog/cancel-each-step",
        test_cancel_on_each_step);
    g_test_add_func("/person-dialog/close-idempotent",
        test_close_and_double_cancel_are_idempotent);
    g_test_add_func("/person-dialog/late-preview",
        test_preview_is_cancelled_and_late_result_ignored);
    g_test_add_func("/person-dialog/complete-once", test_complete_once);
    g_test_add_func("/person-dialog/destruction", test_null_and_full_destruction);
    return g_test_run();
}
