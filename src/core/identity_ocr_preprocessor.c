#include "core/identity_ocr_preprocessor.h"
#include "core/file_hash.h"
#include <glib/gstdio.h>
#include <unistd.h>
#include <poppler.h>

gboolean identity_ocr_preprocessor_dimensions_are_safe(gint width,gint height,
 IdentityOcrPreprocessProfile profile)
{
 if(width<=0||height<=0||width>12000||height>12000||
  (guint64)width*(guint64)height>40000000U||
  profile>IDENTITY_OCR_PREPROCESS_UPSCALE)return FALSE;
 if(profile==IDENTITY_OCR_PREPROCESS_UPSCALE&&(width>6000||height>6000||
  (guint64)width*(guint64)height>10000000U))return FALSE;
 return TRUE;
}

static gboolean apply_profile(const char *path,
    IdentityOcrPreprocessProfile profile, GError **error)
{
    if (profile == IDENTITY_OCR_PREPROCESS_NONE ||
        profile == IDENTITY_OCR_PREPROCESS_ORIENTATION) return TRUE;
    cairo_surface_t *source = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(source) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(source);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "L’image de travail OCR est illisible.");
        return FALSE;
    }
    gint width = cairo_image_surface_get_width(source);
    gint height = cairo_image_surface_get_height(source);
    cairo_surface_t *output = source;
    if (profile == IDENTITY_OCR_PREPROCESS_GRAYSCALE) {
        cairo_surface_flush(source);
        guint8 *pixels = cairo_image_surface_get_data(source);
        gint stride = cairo_image_surface_get_stride(source);
        for (gint y = 0; y < height; y++)
            for (gint x = 0; x < width; x++) {
                guint8 *pixel = pixels + y * stride + x * 4;
                guint8 gray = (guint8) ((pixel[0] * 11U +
                    pixel[1] * 59U + pixel[2] * 30U) / 100U);
                pixel[0] = gray; pixel[1] = gray; pixel[2] = gray;
            }
        cairo_surface_mark_dirty(source);
    } else if (profile == IDENTITY_OCR_PREPROCESS_UPSCALE &&
               identity_ocr_preprocessor_dimensions_are_safe(width,height,
                   profile)) {
        output = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, width * 2, height * 2);
        cairo_t *cr = cairo_create(output);
        cairo_scale(cr, 2.0, 2.0);
        cairo_set_source_surface(cr, source, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_paint(cr); cairo_destroy(cr);
    }
    cairo_status_t status = cairo_surface_write_to_png(output, path);
    if (output != source) cairo_surface_destroy(output);
    cairo_surface_destroy(source);
    if (status != CAIRO_STATUS_SUCCESS) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "Le prétraitement OCR a échoué.");
        return FALSE;
    }
    return TRUE;
}
IdentityOcrWorkImage *identity_ocr_preprocessor_prepare(
 const EvidencePreviewRequest *request,guint pdf_page,
 IdentityOcrPreprocessProfile profile,GCancellable*cancellable,GError**error)
{
 if(request==NULL||pdf_page==0||profile>IDENTITY_OCR_PREPROCESS_UPSCALE){
  g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,
   "Paramètres de prétraitement OCR invalides.");return NULL;}
 EvidencePreviewResult*r=evidence_preview_load(request,cancellable,error);
 if(r==NULL)return NULL;
 if((r->kind!=EVIDENCE_PREVIEW_KIND_IMAGE&&r->kind!=EVIDENCE_PREVIEW_KIND_PDF)||
    r->png_bytes==NULL){evidence_preview_result_free(r);
  g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_SUPPORTED,
   "Cette preuve ne peut pas être préparée pour l’OCR.");return NULL;}
 if(!identity_ocr_preprocessor_dimensions_are_safe(r->width,r->height,profile)){
  evidence_preview_result_free(r);g_set_error_literal(error,G_IO_ERROR,
   G_IO_ERROR_NO_SPACE,"Dimensions OCR hors limites.");return NULL;}
 gchar*path=NULL;gint fd=g_file_open_tmp("labfy-identity-ocr-XXXXXX.png",&path,error);
 if(fd<0){evidence_preview_result_free(r);return NULL;}close(fd);
 if(r->kind==EVIDENCE_PREVIEW_KIND_PDF){
  char*uri=g_filename_to_uri(r->controlled_path,NULL,error);
  PopplerDocument*document=uri!=NULL?
   poppler_document_new_from_file(uri,NULL,error):NULL;g_free(uri);
  gint pages=document!=NULL?poppler_document_get_n_pages(document):0;
  if(document==NULL||pdf_page>(guint)pages){g_clear_object(&document);
   g_unlink(path);g_free(path);evidence_preview_result_free(r);
   if(error!=NULL&&*error==NULL)g_set_error_literal(error,G_IO_ERROR,
    G_IO_ERROR_INVALID_ARGUMENT,"La page PDF choisie n’existe pas.");return NULL;}
  PopplerPage*page=poppler_document_get_page(document,(gint)pdf_page-1);
  double pw=0,ph=0;poppler_page_get_size(page,&pw,&ph);
  if(pw<=0||ph<=0||pw>12000||ph>12000||pw*ph>40000000){
   g_object_unref(page);g_object_unref(document);g_unlink(path);g_free(path);
   evidence_preview_result_free(r);g_set_error_literal(error,G_IO_ERROR,
    G_IO_ERROR_NO_SPACE,"Dimensions PDF hors limites.");return NULL;}
  double scale=MIN(2.0,MIN(2000.0/pw,2000.0/ph));
  gint width=MAX(1,(gint)(pw*scale)),height=MAX(1,(gint)(ph*scale));
  cairo_surface_t*surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
  cairo_t*cr=cairo_create(surface);cairo_set_source_rgb(cr,1,1,1);cairo_paint(cr);
  cairo_scale(cr,scale,scale);poppler_page_render(page,cr);cairo_destroy(cr);
  cairo_status_t status=cairo_surface_write_to_png(surface,path);
  cairo_surface_destroy(surface);g_object_unref(page);g_object_unref(document);
  if(status!=CAIRO_STATUS_SUCCESS){g_unlink(path);g_free(path);
   evidence_preview_result_free(r);g_set_error_literal(error,G_IO_ERROR,
    G_IO_ERROR_FAILED,"Rendu de la page PDF impossible.");return NULL;}
  r->width=width;r->height=height;
 }else{
  gsize length=0;const guint8*data=g_bytes_get_data(r->png_bytes,&length);
  if(!g_file_set_contents(path,(const char*)data,(gssize)length,error)){
   g_unlink(path);g_free(path);evidence_preview_result_free(r);return NULL;}
 }
 if(!apply_profile(path,profile,error)){g_unlink(path);g_free(path);
  evidence_preview_result_free(r);return NULL;}
 IdentityOcrWorkImage*out=g_new0(IdentityOcrWorkImage,1);out->path=path;
 out->width=r->width;out->height=r->height;out->page=pdf_page;
 guint64 size=0;if(!file_hash_compute_sha256(path,cancellable,&out->sha256,
   &size,error)){identity_ocr_work_image_free(out);out=NULL;}
 evidence_preview_result_free(r);return out;
}
void identity_ocr_work_image_free(IdentityOcrWorkImage*i)
{if(!i)return;if(i->path)g_unlink(i->path);g_free(i->path);g_free(i->sha256);g_free(i);}
