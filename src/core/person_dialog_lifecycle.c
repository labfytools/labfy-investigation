#include "core/person_dialog_lifecycle.h"

struct PersonDialogLifecycle
{
    guint64 generation;
    gboolean preview_active;
    gboolean finished;
    gboolean cancelled;
};

PersonDialogLifecycle *person_dialog_lifecycle_new(void)
{
    return g_new0(PersonDialogLifecycle, 1);
}

void person_dialog_lifecycle_free(PersonDialogLifecycle *lifecycle)
{
    g_free(lifecycle);
}

guint64 person_dialog_lifecycle_begin_preview(
    PersonDialogLifecycle *lifecycle)
{
    if (lifecycle == NULL || lifecycle->finished) return 0;
    lifecycle->generation++;
    lifecycle->preview_active = TRUE;
    return lifecycle->generation;
}

gboolean person_dialog_lifecycle_cancel(PersonDialogLifecycle *lifecycle)
{
    if (lifecycle == NULL || lifecycle->finished) return FALSE;
    lifecycle->finished = TRUE;
    lifecycle->cancelled = TRUE;
    lifecycle->preview_active = FALSE;
    lifecycle->generation++;
    return TRUE;
}

gboolean person_dialog_lifecycle_complete(PersonDialogLifecycle *lifecycle)
{
    if (lifecycle == NULL || lifecycle->finished) return FALSE;
    lifecycle->finished = TRUE;
    lifecycle->preview_active = FALSE;
    lifecycle->generation++;
    return TRUE;
}

gboolean person_dialog_lifecycle_accepts_preview(
    const PersonDialogLifecycle *lifecycle, guint64 generation)
{
    return lifecycle != NULL && !lifecycle->finished &&
        lifecycle->preview_active && generation == lifecycle->generation;
}

gboolean person_dialog_lifecycle_is_cancelled(
    const PersonDialogLifecycle *lifecycle)
{
    return lifecycle != NULL && lifecycle->cancelled;
}

gboolean person_dialog_lifecycle_is_finished(
    const PersonDialogLifecycle *lifecycle)
{
    return lifecycle != NULL && lifecycle->finished;
}

gboolean person_dialog_lifecycle_has_preview(
    const PersonDialogLifecycle *lifecycle)
{
    return lifecycle != NULL && lifecycle->preview_active;
}

guint64 person_dialog_lifecycle_get_generation(
    const PersonDialogLifecycle *lifecycle)
{
    return lifecycle != NULL ? lifecycle->generation : 0;
}
