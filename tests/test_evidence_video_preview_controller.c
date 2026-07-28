#include "core/evidence_video_preview_controller.h"
#include <glib.h>
typedef struct { guint pause, seek, detach, release; } Effects;
typedef struct { Effects *effects; } FakeMedia;
static void pause_media(gpointer data)
{ ((FakeMedia *) data)->effects->pause++; }
static void seek_media(gpointer data)
{ ((FakeMedia *) data)->effects->seek++; }
static void detach_media(gpointer data)
{ ((Effects *) data)->detach++; }
static void release_media(gpointer data)
{ ((FakeMedia *) data)->effects->release++; g_free(data); }
static FakeMedia *media_new(Effects *effects)
{ FakeMedia *media = g_new0(FakeMedia, 1); media->effects = effects; return media; }
static void test_replace_close(void)
{
    Effects effects = {0};
    EvidenceVideoPreviewActions actions = {
        pause_media, seek_media, detach_media, release_media};
    EvidenceVideoPreviewController *controller =
        evidence_video_preview_controller_new(&effects, &actions);
    g_assert_false(evidence_video_preview_controller_has_media(controller));
    evidence_video_preview_controller_replace(controller, media_new(&effects));
    g_assert_true(evidence_video_preview_controller_has_media(controller));
    evidence_video_preview_controller_replace(controller, media_new(&effects));
    g_assert_cmpuint(effects.pause, ==, 1);
    g_assert_cmpuint(effects.seek, ==, 1);
    g_assert_cmpuint(effects.detach, ==, 1);
    g_assert_cmpuint(effects.release, ==, 1);
    evidence_video_preview_controller_free(controller);
    g_assert_cmpuint(effects.pause, ==, 2);
    g_assert_cmpuint(effects.seek, ==, 2);
    g_assert_cmpuint(effects.detach, ==, 2);
    g_assert_cmpuint(effects.release, ==, 2);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-video/replace-close", test_replace_close);
    return g_test_run();
}
