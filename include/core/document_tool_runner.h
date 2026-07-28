/******************************************************************************
 * @file document_tool_runner.h
 * @brief Exécution bornée et annulable des outils documentaires.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_DOCUMENT_TOOL_RUNNER_H
#define LABFY_INVESTIGATION_DOCUMENT_TOOL_RUNNER_H

#include "core/document_analysis.h"

G_BEGIN_DECLS

gboolean document_tool_runner_run(
    const char *tool_id,
    const char *executable,
    const char *const arguments[],
    const char *source_path,
    GCancellable *cancellable,
    DocumentToolExecution **out_execution,
    GError **error
);

char *document_tool_runner_read_version(
    const char *executable,
    const char *const arguments[],
    GCancellable *cancellable
);

G_END_DECLS
#endif
