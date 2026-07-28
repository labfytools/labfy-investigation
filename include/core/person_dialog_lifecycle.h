#ifndef LABFY_INVESTIGATION_PERSON_DIALOG_LIFECYCLE_H
#define LABFY_INVESTIGATION_PERSON_DIALOG_LIFECYCLE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct PersonDialogLifecycle PersonDialogLifecycle;

PersonDialogLifecycle *person_dialog_lifecycle_new(void);
void person_dialog_lifecycle_free(PersonDialogLifecycle *lifecycle);
guint64 person_dialog_lifecycle_begin_preview(
    PersonDialogLifecycle *lifecycle);
gboolean person_dialog_lifecycle_cancel(PersonDialogLifecycle *lifecycle);
gboolean person_dialog_lifecycle_complete(PersonDialogLifecycle *lifecycle);
gboolean person_dialog_lifecycle_accepts_preview(
    const PersonDialogLifecycle *lifecycle, guint64 generation);
gboolean person_dialog_lifecycle_is_cancelled(
    const PersonDialogLifecycle *lifecycle);
gboolean person_dialog_lifecycle_is_finished(
    const PersonDialogLifecycle *lifecycle);
gboolean person_dialog_lifecycle_has_preview(
    const PersonDialogLifecycle *lifecycle);
guint64 person_dialog_lifecycle_get_generation(
    const PersonDialogLifecycle *lifecycle);

G_END_DECLS

#endif
