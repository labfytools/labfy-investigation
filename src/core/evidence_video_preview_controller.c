#include "core/evidence_video_preview_controller.h"
struct EvidenceVideoPreviewController {
    gpointer owner;
    gpointer media;
    EvidenceVideoPreviewActions actions;
};
EvidenceVideoPreviewController *evidence_video_preview_controller_new(
    gpointer owner, const EvidenceVideoPreviewActions *actions)
{
    if (actions == NULL || actions->pause == NULL ||
        actions->detach == NULL || actions->release == NULL) return NULL;
    EvidenceVideoPreviewController *controller = g_new0(
        EvidenceVideoPreviewController, 1);
    controller->owner = owner; controller->actions = *actions;
    return controller;
}
void evidence_video_preview_controller_stop(
    EvidenceVideoPreviewController *controller)
{
    if (controller == NULL || controller->media == NULL) return;
    controller->actions.pause(controller->media);
    if (controller->actions.seek_start != NULL)
        controller->actions.seek_start(controller->media);
    if (controller->owner != NULL)
        controller->actions.detach(controller->owner);
    controller->actions.release(controller->media);
    controller->media = NULL;
}
void evidence_video_preview_controller_replace(
    EvidenceVideoPreviewController *controller, gpointer media)
{
    if (controller == NULL) return;
    evidence_video_preview_controller_stop(controller);
    controller->media = media;
}
void evidence_video_preview_controller_free(
    EvidenceVideoPreviewController *controller)
{
    if (controller == NULL) return;
    evidence_video_preview_controller_stop(controller); g_free(controller);
}
void evidence_video_preview_controller_abandon_owner(
    EvidenceVideoPreviewController *controller)
{
    if (controller == NULL) return;
    controller->owner = NULL;
}
gboolean evidence_video_preview_controller_has_media(
    const EvidenceVideoPreviewController *controller)
{ return controller != NULL && controller->media != NULL; }
