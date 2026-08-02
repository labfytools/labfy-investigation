#ifndef LABFY_PERSON_CREATION_CONFIRMATION_H
#define LABFY_PERSON_CREATION_CONFIRMATION_H

#include "models/identity_ocr.h"
#include "views/person_factual_relation_editor.h"
#include "views/person_ocr_projection_editor.h"
#include <glib.h>

G_BEGIN_DECLS

void person_creation_confirmation_append_sections(
    GString *summary, const GPtrArray *ocr_runs,
    PersonOcrProjectionEditor *projection_editor,
    PersonFactualRelationEditor *relation_editor);

G_END_DECLS

#endif
