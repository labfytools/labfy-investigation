#ifndef LABFY_EVIDENCE_VIDEO_PREVIEW_CONTROLLER_H
#define LABFY_EVIDENCE_VIDEO_PREVIEW_CONTROLLER_H
#include <glib.h>
G_BEGIN_DECLS
typedef struct EvidenceVideoPreviewController EvidenceVideoPreviewController;
typedef struct {
    void (*pause)(gpointer media);
    void (*seek_start)(gpointer media);
    void (*detach)(gpointer owner);
    void (*release)(gpointer media);
} EvidenceVideoPreviewActions;
EvidenceVideoPreviewController *evidence_video_preview_controller_new(
    gpointer owner, const EvidenceVideoPreviewActions *actions);
void evidence_video_preview_controller_replace(
    EvidenceVideoPreviewController *controller, gpointer media);
void evidence_video_preview_controller_stop(
    EvidenceVideoPreviewController *controller);
void evidence_video_preview_controller_free(
    EvidenceVideoPreviewController *controller);
void evidence_video_preview_controller_abandon_owner(
    EvidenceVideoPreviewController *controller);
gboolean evidence_video_preview_controller_has_media(
    const EvidenceVideoPreviewController *controller);
G_END_DECLS
#endif
