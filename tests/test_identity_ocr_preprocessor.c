#include "core/identity_ocr_preprocessor.h"
#include "core/file_hash.h"
#include "core/ocr_analysis.h"
#include "core/tool_registry.h"
#include <cairo.h>
#include <cairo-pdf.h>
#include <glib/gstdio.h>
#include <jpeglib.h>
#include <libheif/heif.h>
#include <stdio.h>
#include <string.h>

static void remove_fixture(char *root, char *path, char *sha)
{
    g_unlink(path); g_rmdir(root); g_free(path); g_free(root); g_free(sha);
}

static void test_png_profiles_integrity_cleanup(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-ocr-preprocess-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.png", NULL);
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_RGB24, 80, 40);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1); cairo_paint(cr);
    cairo_set_source_rgb(cr, 0, 0, 0); cairo_move_to(cr, 4, 24);
    cairo_show_text(cr, "SPECIMEN");
    g_assert_cmpint(cairo_surface_write_to_png(surface, path), ==,
        CAIRO_STATUS_SUCCESS);
    cairo_destroy(cr); cairo_surface_destroy(surface);
    char *sha = NULL; guint64 size = 0;
    g_assert_true(file_hash_compute_sha256(path, NULL, &sha, &size, &error));
    for (guint profile = IDENTITY_OCR_PREPROCESS_NONE;
         profile <= IDENTITY_OCR_PREPROCESS_UPSCALE; profile++) {
        EvidencePreviewRequest *request = evidence_preview_request_new(root,
            "10000000-0000-4000-8000-000000000109", "SPECIMEN.png", sha,
            "image/png", profile);
        IdentityOcrWorkImage *image = identity_ocr_preprocessor_prepare(
            request, 1, profile, NULL, &error);
        g_assert_no_error(error); g_assert_nonnull(image);
        g_assert_cmpuint(strlen(image->sha256), ==, 64);
        char *work_path = g_strdup(image->path);
        identity_ocr_work_image_free(image);
        g_assert_false(g_file_test(work_path, G_FILE_TEST_EXISTS));
        g_free(work_path); evidence_preview_request_free(request);
    }
    char *after = NULL; guint64 after_size = 0;
    g_assert_true(file_hash_compute_sha256(path, NULL, &after, &after_size,
        &error));
    g_assert_cmpstr(after, ==, sha); g_free(after);
    EvidencePreviewRequest *bad = evidence_preview_request_new(root,
        "10000000-0000-4000-8000-000000000109", "SPECIMEN.png",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "image/png", 1);
    g_assert_null(identity_ocr_preprocessor_prepare(bad, 1,
        IDENTITY_OCR_PREPROCESS_NONE, NULL, &error));
    g_assert_nonnull(error); g_clear_error(&error);
    evidence_preview_request_free(bad);
    GCancellable *cancel = g_cancellable_new(); g_cancellable_cancel(cancel);
    bad = evidence_preview_request_new(root,
        "10000000-0000-4000-8000-000000000109", "SPECIMEN.png", sha,
        "image/png", 1);
    g_assert_null(identity_ocr_preprocessor_prepare(bad, 1,
        IDENTITY_OCR_PREPROCESS_NONE, cancel, &error));
    g_assert_nonnull(error);
    g_clear_error(&error); g_object_unref(cancel);
    evidence_preview_request_free(bad);
    remove_fixture(root, path, sha);
}

static void test_missing(void)
{
    EvidencePreviewRequest *request = evidence_preview_request_new("/tmp",
        "10000000-0000-4000-8000-000000000109", "ABSENT-SPECIMEN.png",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "image/png", 1);
    GError *error = NULL;
    g_assert_null(identity_ocr_preprocessor_prepare(request, 1,
        IDENTITY_OCR_PREPROCESS_NONE, NULL, &error));
    g_assert_nonnull(error); g_clear_error(&error);
    evidence_preview_request_free(request);
}
static IdentityOcrWorkImage *prepare_path(const char*root,const char*name,
 const char*mime,guint page,IdentityOcrPreprocessProfile profile,char**sha)
{
 GError*error=NULL;guint64 size=0;char*path=g_build_filename(root,name,NULL);
 g_assert_true(file_hash_compute_sha256(path,NULL,sha,&size,&error));
 EvidencePreviewRequest*r=evidence_preview_request_new(root,
  "10000000-0000-4000-8000-000000000119",name,*sha,mime,1);
 IdentityOcrWorkImage*out=identity_ocr_preprocessor_prepare(r,page,profile,
  NULL,&error);g_assert_no_error(error);g_assert_nonnull(out);
 evidence_preview_request_free(r);g_free(path);return out;
}
static void write_jpeg(const char*path,guint orientation)
{
 FILE*out=fopen(path,"wb");struct jpeg_compress_struct c;
 struct jpeg_error_mgr e;c.err=jpeg_std_error(&e);jpeg_create_compress(&c);
 jpeg_stdio_dest(&c,out);c.image_width=96;c.image_height=48;
 c.input_components=3;c.in_color_space=JCS_RGB;jpeg_set_defaults(&c);
 jpeg_start_compress(&c,TRUE);
 if(orientation>0){guint8 exif[]={ 'E','x','i','f',0,0,'I','I',42,0,8,0,0,0,
  1,0,0x12,1,3,0,1,0,0,0,0,0,0,0,0,0,0,0};
  exif[24]=(guint8)orientation;jpeg_write_marker(&c,JPEG_APP0+1,exif,sizeof(exif));}
 JSAMPLE row[96*3];for(guint x=0;x<96;x++){row[x*3]=x<48?255:0;
  row[x*3+1]=x<48?0:255;row[x*3+2]=32;}
 while(c.next_scanline<c.image_height){JSAMPROW rows[]={row};
  jpeg_write_scanlines(&c,rows,1);}jpeg_finish_compress(&c);
 jpeg_destroy_compress(&c);fclose(out);
}
static void test_jpeg_and_exif(void)
{
 GError*error=NULL;char*root=g_dir_make_tmp("labfy-ocr-jpeg-XXXXXX",&error);
 const guint orientations[]={0,1,3,6,8,9};
 for(guint i=0;i<G_N_ELEMENTS(orientations);i++){char*name=g_strdup_printf(
   "SPECIMEN-%u.bin",orientations[i]);char*path=g_build_filename(root,name,NULL);
  write_jpeg(path,orientations[i]);char*sha=NULL;IdentityOcrWorkImage*image=
   prepare_path(root,name,NULL,1,IDENTITY_OCR_PREPROCESS_ORIENTATION,&sha);
  if(orientations[i]==6||orientations[i]==8){g_assert_cmpint(image->width,==,48);
   g_assert_cmpint(image->height,==,96);}else{g_assert_cmpint(image->width,==,96);
   g_assert_cmpint(image->height,==,48);}
  identity_ocr_work_image_free(image);g_unlink(path);g_free(sha);g_free(path);g_free(name);}
 g_rmdir(root);g_free(root);
}
static gboolean write_heif(const char*path)
{
 struct heif_context*c=heif_context_alloc();struct heif_encoder*encoder=NULL;
 struct heif_image*image=NULL;struct heif_image_handle*handle=NULL;
 struct heif_error e=heif_context_get_encoder_for_format(c,
  heif_compression_HEVC,&encoder);if(e.code!=heif_error_Ok){heif_context_free(c);return FALSE;}
 e=heif_image_create(64,40,heif_colorspace_RGB,heif_chroma_interleaved_RGB,&image);
 if(e.code==heif_error_Ok)e=heif_image_add_plane(image,heif_channel_interleaved,64,40,8);
 if(e.code==heif_error_Ok){gint stride=0;guint8*p=heif_image_get_plane(image,
   heif_channel_interleaved,&stride);memset(p,127,(gsize)stride*40);
  e=heif_context_encode_image(c,image,encoder,NULL,&handle);}
 if(e.code==heif_error_Ok)e=heif_context_write_to_file(c,path);
 if(handle)heif_image_handle_release(handle);
 if(image)heif_image_release(image);
 heif_encoder_release(encoder);heif_context_free(c);return e.code==heif_error_Ok;
}
static void test_heic_heif(void)
{
 GError*error=NULL;char*root=g_dir_make_tmp("labfy-ocr-heif-XXXXXX",&error);
 char*heic=g_build_filename(root,"SPECIMEN",NULL);
 if(!write_heif(heic)){g_test_skip("Encodeur HEVC indisponible.");g_rmdir(root);
  g_free(heic);g_free(root);return;}
 char*contents=NULL;gsize length=0;
 g_assert_true(g_file_get_contents(heic,&contents,&length,&error));
 g_assert_no_error(error);
 char*sha=NULL;IdentityOcrWorkImage*image=prepare_path(root,"SPECIMEN",NULL,1,
 IDENTITY_OCR_PREPROCESS_NONE,&sha);g_assert_cmpint(image->width,==,64);
 identity_ocr_work_image_free(image);g_free(sha);sha=NULL;
 for(gsize i=0;i+4<=length;i++)if(memcmp(contents+i,"heic",4)==0)memcpy(contents+i,"mif1",4);
 char*heif=g_build_filename(root,"SPECIMEN.heif",NULL);
 g_assert_true(g_file_set_contents(heif,contents,(gssize)length,&error));
 g_assert_no_error(error);
 image=prepare_path(root,"SPECIMEN.heif",NULL,1,
  IDENTITY_OCR_PREPROCESS_GRAYSCALE,&sha);g_assert_cmpint(image->height,==,40);
 identity_ocr_work_image_free(image);g_free(sha);g_free(contents);
 g_unlink(heic);g_unlink(heif);g_free(heic);g_free(heif);g_rmdir(root);g_free(root);
}
static void test_pdf_pages(void)
{
 GError*error=NULL;char*root=g_dir_make_tmp("labfy-ocr-pdf-XXXXXX",&error);
 char*path=g_build_filename(root,"SPECIMEN.pdf",NULL);
 cairo_surface_t*s=cairo_pdf_surface_create(path,320,200);cairo_t*cr=cairo_create(s);
 cairo_set_source_rgb(cr,1,0,0);cairo_paint(cr);cairo_show_page(cr);
 cairo_set_source_rgb(cr,0,0,1);cairo_paint(cr);cairo_show_page(cr);
 cairo_destroy(cr);cairo_surface_destroy(s);
 char*sha1=NULL,*sha2=NULL;IdentityOcrWorkImage*p1=prepare_path(root,
  "SPECIMEN.pdf","application/pdf",1,IDENTITY_OCR_PREPROCESS_NONE,&sha1);
 IdentityOcrWorkImage*p2=prepare_path(root,"SPECIMEN.pdf","application/pdf",2,
  IDENTITY_OCR_PREPROCESS_UPSCALE,&sha2);
 g_assert_cmpstr(p1->sha256,!=,p2->sha256);
 identity_ocr_work_image_free(p1);identity_ocr_work_image_free(p2);
 guint64 size=0;char*source_after=NULL;file_hash_compute_sha256(path,NULL,
  &source_after,&size,&error);g_assert_cmpstr(source_after,==,sha1);
 g_free(source_after);g_free(sha1);g_free(sha2);g_unlink(path);g_free(path);
 g_rmdir(root);g_free(root);
}
static void test_direct_limits(void)
{
 g_assert_false(identity_ocr_preprocessor_dimensions_are_safe(12001,1,
  IDENTITY_OCR_PREPROCESS_NONE));
 g_assert_false(identity_ocr_preprocessor_dimensions_are_safe(1,12001,
  IDENTITY_OCR_PREPROCESS_NONE));
 g_assert_false(identity_ocr_preprocessor_dimensions_are_safe(8001,5000,
  IDENTITY_OCR_PREPROCESS_NONE));
 g_assert_false(identity_ocr_preprocessor_dimensions_are_safe(0,1,
  IDENTITY_OCR_PREPROCESS_NONE));
 g_assert_false(identity_ocr_preprocessor_dimensions_are_safe(-1,1,
  IDENTITY_OCR_PREPROCESS_NONE));
 g_assert_false(identity_ocr_preprocessor_dimensions_are_safe(6001,1,
  IDENTITY_OCR_PREPROCESS_UPSCALE));
 g_assert_true(identity_ocr_preprocessor_dimensions_are_safe(2000,1000,
  IDENTITY_OCR_PREPROCESS_UPSCALE));
}

static void test_real_tesseract_optional(void)
{
    GError *error = NULL;
    ToolRegistry *registry = tool_registry_new();
    g_assert_true(tool_registry_register(registry, "ocr.tesseract",
        "Tesseract OCR", "tesseract", TOOL_REQUIREMENT_OPTIONAL, &error));
    g_assert_true(tool_registry_refresh(registry, &error));
    const ToolInfo *tool = tool_registry_find(registry, "ocr.tesseract");
    if (tool == NULL || tool_info_get_availability(tool) !=
        TOOL_AVAILABILITY_AVAILABLE) {
        g_test_skip("Tesseract absent.");
        tool_registry_free(registry); return;
    }
    char *raw = ocr_analysis_list_languages(
        tool_info_get_resolved_path(tool), NULL, &error);
    if (raw == NULL) {
        g_test_skip("Tesseract ne peut pas lister ses langues.");
        g_clear_error(&error); tool_registry_free(registry); return;
    }
    GPtrArray *parsed = ocr_analysis_parse_languages(raw);
    const char *language = NULL;
    for (guint i = 0; i < parsed->len; i++) {
        const char *candidate = g_ptr_array_index(parsed, i);
        if (g_str_equal(candidate, "eng") || g_str_equal(candidate, "fra")) {
            language = candidate; break;
        }
    }
    if (language == NULL) {
        g_test_skip("Aucune langue eng/fra installée.");
        g_ptr_array_unref(parsed); g_free(raw);
        tool_registry_free(registry); return;
    }
    char *root = g_dir_make_tmp("labfy-real-tesseract-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.png", NULL);
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_RGB24, 1000, 260);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1); cairo_paint(cr);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 100); cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, 40, 160); cairo_show_text(cr, "SPECIMEN 109");
    cairo_surface_write_to_png(surface, path);
    cairo_destroy(cr); cairo_surface_destroy(surface);
    char *sha = NULL; guint64 size = 0;
    g_assert_true(file_hash_compute_sha256(path, NULL, &sha, &size, &error));
    OcrAnalysisResult *result = ocr_analysis_run(
        tool_info_get_resolved_path(tool), path, language, NULL, &error);
    if (result == NULL) {
        g_test_skip("Tesseract ne peut pas initialiser la fixture.");
        g_clear_error(&error);
    } else {
        g_assert_nonnull(result->text); g_assert_cmpuint(strlen(result->text),>,0);
        g_assert_nonnull(result->tsv); g_assert_nonnull(strstr(result->tsv,"left"));
        g_assert_nonnull(result->execution->version);
        g_assert_cmpstr(result->requested_languages, ==, language);
        g_assert_cmpuint(strlen(sha), ==, 64);
        ocr_analysis_result_free(result);
    }
    g_unlink(path); g_rmdir(root); g_free(path); g_free(root); g_free(sha);
    g_ptr_array_unref(parsed); g_free(raw); tool_registry_free(registry);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/identity-preprocessor/png-profiles-integrity-cleanup",
        test_png_profiles_integrity_cleanup);
    g_test_add_func("/identity-preprocessor/missing", test_missing);
    g_test_add_func("/identity-preprocessor/jpeg-exif",test_jpeg_and_exif);
    g_test_add_func("/identity-preprocessor/heic-heif",test_heic_heif);
    g_test_add_func("/identity-preprocessor/pdf-pages",test_pdf_pages);
    g_test_add_func("/identity-preprocessor/limits",test_direct_limits);
    g_test_add_func("/identity-preprocessor/real-tesseract",
        test_real_tesseract_optional);
    return g_test_run();
}
