#include "core/evidence_preview.h"
#include "core/evidence_integrity_verifier.h"
#include "core/eml_analyzer.h"
#include "core/eml_mime_extractor.h"
#include <cairo.h>
#include <libheif/heif.h>
#include <poppler.h>
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <string.h>

typedef enum {
    FORMAT_UNKNOWN, FORMAT_PNG, FORMAT_JPEG, FORMAT_HEIF, FORMAT_HEIC,
    FORMAT_MP4, FORMAT_MOV, FORMAT_PDF, FORMAT_EML, FORMAT_TEXT
} PreviewFormat;
typedef struct { struct jpeg_error_mgr base; jmp_buf jump; } PreviewJpegError;

static GQuark preview_error(void)
{ return g_quark_from_static_string("evidence-preview-error"); }
static void jpeg_failed(j_common_ptr info)
{ longjmp(((PreviewJpegError *) info->err)->jump, 1); }
static cairo_status_t write_png(void *closure, const unsigned char *data,
    unsigned int length)
{ g_byte_array_append(closure, data, length); return CAIRO_STATUS_SUCCESS; }
static gboolean cancelled(GCancellable *cancellable, GError **error)
{ return cancellable != NULL &&
    g_cancellable_set_error_if_cancelled(cancellable, error); }
static gboolean brand_is(const guint8 *data, gsize size, const char *brand)
{
    if (size < 12 || memcmp(data + 4, "ftyp", 4) != 0) return FALSE;
    for (gsize offset = 8; offset + 4 <= size; offset += 4)
        if (memcmp(data + offset, brand, 4) == 0) return TRUE;
    return FALSE;
}
static gboolean looks_like_eml(const guint8 *data, gsize size)
{
    char *text = g_strndup((const char *) data, MIN(size, 65536U));
    gboolean result = (g_str_has_prefix(text, "From:") ||
        strstr(text, "\nFrom:") != NULL) &&
        (strstr(text, "\nSubject:") != NULL ||
         strstr(text, "\nDate:") != NULL) &&
        (strstr(text, "\r\n\r\n") != NULL || strstr(text, "\n\n") != NULL);
    g_free(text); return result;
}
static gboolean looks_textual(const guint8 *data, gsize size)
{
    if (memchr(data, '\0', size) != NULL) return FALSE;
    gsize invalid = 0;
    for (gsize i = 0; i < size; i++)
        if (data[i] < 0x09 || (data[i] > 0x0d && data[i] < 0x20)) invalid++;
    return size == 0 || invalid * 100U <= size;
}
static PreviewFormat detect_format(const guint8 *data, gsize size,
    const char *path, const char *historical_mime)
{
    if (size >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0)
        return FORMAT_PNG;
    if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff)
        return FORMAT_JPEG;
    if (size >= 5 && memcmp(data, "%PDF-", 5) == 0) return FORMAT_PDF;
    if (brand_is(data, size, "heic") || brand_is(data, size, "heix") ||
        brand_is(data, size, "hevc") || brand_is(data, size, "hevx"))
        return FORMAT_HEIC;
    if (brand_is(data, size, "mif1") || brand_is(data, size, "msf1") ||
        brand_is(data, size, "heif")) return FORMAT_HEIF;
    if (brand_is(data, size, "qt  ")) return FORMAT_MOV;
    if (brand_is(data, size, "isom") || brand_is(data, size, "mp41") ||
        brand_is(data, size, "mp42") || brand_is(data, size, "avc1"))
        return FORMAT_MP4;
    if (looks_like_eml(data, size)) return FORMAT_EML;
    if (looks_textual(data, size)) {
        const char *suffix = strrchr(path, '.');
        gboolean misleading = suffix != NULL &&
            (g_ascii_strcasecmp(suffix, ".png") == 0 ||
             g_ascii_strcasecmp(suffix, ".jpg") == 0 ||
             g_ascii_strcasecmp(suffix, ".jpeg") == 0 ||
             g_ascii_strcasecmp(suffix, ".heic") == 0 ||
             g_ascii_strcasecmp(suffix, ".heif") == 0 ||
             g_ascii_strcasecmp(suffix, ".mp4") == 0 ||
             g_ascii_strcasecmp(suffix, ".mov") == 0 ||
             g_ascii_strcasecmp(suffix, ".pdf") == 0 ||
             g_ascii_strcasecmp(suffix, ".eml") == 0);
        if (misleading || (historical_mime != NULL &&
            !g_str_has_prefix(historical_mime, "text/"))) return FORMAT_UNKNOWN;
        char *content_type = g_content_type_guess(path, data, size, NULL);
        char *mime = content_type != NULL
            ? g_content_type_get_mime_type(content_type) : NULL;
        gboolean indicated = mime != NULL && g_str_has_prefix(mime, "text/");
        indicated = indicated || (historical_mime != NULL &&
            g_str_has_prefix(historical_mime, "text/"));
        g_free(mime); g_free(content_type);
        if (indicated || size == 0) return FORMAT_TEXT;
    }
    return FORMAT_UNKNOWN;
}
static EvidencePreviewResult *result_new(const EvidencePreviewRequest *request,
    const char *path, guint64 size)
{
    EvidencePreviewResult *result = g_new0(EvidencePreviewResult, 1);
    result->kind = EVIDENCE_PREVIEW_KIND_UNSUPPORTED;
    result->evidence_identifier = g_strdup(request->evidence_identifier);
    result->request_generation = request->request_generation;
    result->display_name = g_path_get_basename(path);
    result->controlled_path = g_strdup(path);
    result->size_bytes = size;
    result->integrity_valid = TRUE;
    return result;
}
static gboolean surface_to_result(cairo_surface_t *source, gint source_width,
    gint source_height, EvidencePreviewResult *result, GError **error)
{
    double scale = MIN(1.0, MIN((double) EVIDENCE_PREVIEW_MAX_EDGE /
        source_width, (double) EVIDENCE_PREVIEW_MAX_EDGE / source_height));
    gint width = MAX(1, (gint) (source_width * scale));
    gint height = MAX(1, (gint) (source_height * scale));
    cairo_surface_t *target = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(target);
    GByteArray *bytes = g_byte_array_new();
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, source, 0, 0); cairo_paint(cr);
    cairo_destroy(cr);
    if (cairo_surface_write_to_png_stream(target, write_png, bytes) !=
        CAIRO_STATUS_SUCCESS) {
        g_set_error_literal(error, preview_error(), 4,
            "Impossible de produire l'aperçu en mémoire.");
        g_byte_array_unref(bytes); cairo_surface_destroy(target); return FALSE;
    }
    cairo_surface_destroy(target);
    result->width = width; result->height = height;
    result->png_bytes = g_byte_array_free_to_bytes(bytes);
    result->preview_available = TRUE;
    return TRUE;
}
static gboolean valid_dimensions(gint width, gint height, GError **error)
{
    if (!evidence_preview_dimensions_are_safe(width, height)) {
        g_set_error_literal(error, preview_error(), 5,
            "Les dimensions dépassent les limites sûres.");
        return FALSE;
    }
    return TRUE;
}
static guint16 read_u16(const guint8*p,gboolean little)
{return little?(guint16)(p[0]|p[1]<<8):(guint16)(p[0]<<8|p[1]);}
static guint32 read_u32(const guint8*p,gboolean little)
{return little?((guint32)p[0]|(guint32)p[1]<<8|(guint32)p[2]<<16|(guint32)p[3]<<24):
 ((guint32)p[0]<<24|(guint32)p[1]<<16|(guint32)p[2]<<8|p[3]);}
static gint jpeg_exif_orientation(const char*path)
{
 char*raw=NULL;gsize size=0;if(!g_file_get_contents(path,&raw,&size,NULL))return 1;
 const guint8*d=(const guint8*)raw;gint orientation=1;
 for(gsize p=2;p+4<size&&d[p]==0xff;){
  guint8 marker=d[p+1];guint16 length=(guint16)(d[p+2]<<8|d[p+3]);
  if(length<2||p+2+length>size)break;
  if(marker==0xe1&&length>=16&&memcmp(d+p+4,"Exif\0\0",6)==0){
   const guint8*t=d+p+10;gsize available=p+2+length-(p+10);
   gboolean little=available>=8&&t[0]=='I'&&t[1]=='I';
   if((little||(t[0]=='M'&&t[1]=='M'))&&read_u16(t+2,little)==42){
    guint32 offset=read_u32(t+4,little);
    if(offset+2<=available){guint16 count=read_u16(t+offset,little);
     for(guint i=0;i<count&&offset+2+(i+1)*12<=available;i++){
      const guint8*entry=t+offset+2+i*12;
      if(read_u16(entry,little)==0x0112){
       guint16 value=read_u16(entry+8,little);
       if(value==1||value==3||value==6||value==8)orientation=value;
      }}}
   }break;
  }p+=2+length;
 }g_free(raw);return orientation;
}
static cairo_surface_t *orient_surface(cairo_surface_t*source,gint orientation,
 gint width,gint height,gint*out_width,gint*out_height)
{
 if(orientation==1){*out_width=width;*out_height=height;return source;}
 gboolean quarter=orientation==6||orientation==8;
 *out_width=quarter?height:width;*out_height=quarter?width:height;
 cairo_surface_t*out=cairo_image_surface_create(CAIRO_FORMAT_RGB24,
  *out_width,*out_height);cairo_t*cr=cairo_create(out);
 if(orientation==3){cairo_translate(cr,width,height);cairo_rotate(cr,G_PI);}
 else if(orientation==6){cairo_translate(cr,height,0);cairo_rotate(cr,G_PI_2);}
 else if(orientation==8){cairo_translate(cr,0,width);cairo_rotate(cr,-G_PI_2);}
 cairo_set_source_surface(cr,source,0,0);cairo_paint(cr);cairo_destroy(cr);
 cairo_surface_destroy(source);return out;
}
gboolean evidence_preview_dimensions_are_safe(gint width, gint height)
{
    return width > 0 && height > 0 &&
        width <= EVIDENCE_PREVIEW_MAX_DIMENSION &&
        height <= EVIDENCE_PREVIEW_MAX_DIMENSION &&
        (guint64) width * (guint64) height <= EVIDENCE_PREVIEW_MAX_PIXELS;
}
static gboolean load_png(const char *path, EvidencePreviewResult *result,
    GError **error)
{
    cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
    gint width = cairo_image_surface_get_width(surface);
    gint height = cairo_image_surface_get_height(surface);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS ||
        !valid_dimensions(width, height, error)) {
        if (error != NULL && *error == NULL) g_set_error_literal(error,
            preview_error(), 4, "Le fichier PNG est illisible ou corrompu.");
        cairo_surface_destroy(surface); return FALSE;
    }
    gboolean ok = surface_to_result(surface, width, height, result, error);
    cairo_surface_destroy(surface); return ok;
}
static gboolean load_jpeg(const char *path, EvidencePreviewResult *result,
    GCancellable *cancellable, GError **error)
{
    FILE *file = fopen(path, "rb"); struct jpeg_decompress_struct jpeg = {0};
    PreviewJpegError jpeg_error; cairo_surface_t *surface = NULL;
    if (file == NULL) goto invalid;
    jpeg.err = jpeg_std_error(&jpeg_error.base);
    jpeg_error.base.error_exit = jpeg_failed;
    if (setjmp(jpeg_error.jump) != 0) {
        jpeg_destroy_decompress(&jpeg); fclose(file);
        if (surface != NULL) cairo_surface_destroy(surface);
        goto invalid;
    }
    jpeg_create_decompress(&jpeg); jpeg_stdio_src(&jpeg, file);
    jpeg_read_header(&jpeg, TRUE);
    gint width = (gint) jpeg.image_width, height = (gint) jpeg.image_height;
    if (!valid_dimensions(width, height, error)) {
        jpeg_destroy_decompress(&jpeg); fclose(file); return FALSE;
    }
    jpeg.out_color_space = JCS_RGB; jpeg_start_decompress(&jpeg);
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    unsigned char *pixels = cairo_image_surface_get_data(surface);
    gint stride = cairo_image_surface_get_stride(surface);
    JSAMPARRAY row = (*jpeg.mem->alloc_sarray)((j_common_ptr) &jpeg,
        JPOOL_IMAGE, (JDIMENSION) width * 3U, 1);
    while (jpeg.output_scanline < jpeg.output_height) {
        jpeg_read_scanlines(&jpeg, row, 1);
        guint32 *destination = (guint32 *) (pixels +
            (jpeg.output_scanline - 1) * stride);
        for (gint x = 0; x < width; x++) destination[x] =
            ((guint32) row[0][x * 3] << 16) |
            ((guint32) row[0][x * 3 + 1] << 8) | row[0][x * 3 + 2];
        if (cancellable != NULL && g_cancellable_is_cancelled(cancellable))
            break;
    }
    cairo_surface_mark_dirty(surface); jpeg_finish_decompress(&jpeg);
    jpeg_destroy_decompress(&jpeg); fclose(file);
    if (cancelled(cancellable, error)) {
        cairo_surface_destroy(surface); return FALSE;
    }
    surface=orient_surface(surface,jpeg_exif_orientation(path),width,height,
        &width,&height);
    gboolean ok = surface_to_result(surface, width, height, result, error);
    cairo_surface_destroy(surface); return ok;
invalid:
    g_set_error_literal(error, preview_error(), 4,
        "Le fichier JPEG est illisible ou corrompu."); return FALSE;
}
static gboolean load_heif(const char *path, EvidencePreviewResult *result,
    GError **error)
{
    struct heif_context *context = heif_context_alloc();
    struct heif_image_handle *handle = NULL; struct heif_image *image = NULL;
    struct heif_error heif_error = heif_context_read_from_file(
        context, path, NULL);
    if (heif_error.code == heif_error_Ok)
        heif_error = heif_context_get_primary_image_handle(context, &handle);
    gint width = handle != NULL ? heif_image_handle_get_width(handle) : 0;
    gint height = handle != NULL ? heif_image_handle_get_height(handle) : 0;
    result->item_count = (guint)
        MAX(0, heif_context_get_number_of_top_level_images(context));
    if (heif_error.code != heif_error_Ok ||
        !valid_dimensions(width, height, error)) goto failed;
    heif_error = heif_decode_image(handle, &image, heif_colorspace_RGB,
        heif_chroma_interleaved_RGBA, NULL);
    if (heif_error.code != heif_error_Ok) goto failed;
    gint stride = 0;
    const guint8 *plane = heif_image_get_plane_readonly(image,
        heif_channel_interleaved, &stride);
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height);
    guint8 *target = cairo_image_surface_get_data(surface);
    gint target_stride = cairo_image_surface_get_stride(surface);
    for (gint y = 0; y < height; y++) {
        guint32 *row = (guint32 *) (target + y * target_stride);
        for (gint x = 0; x < width; x++) {
            const guint8 *pixel = plane + y * stride + x * 4;
            row[x] = ((guint32) pixel[3] << 24) |
                ((guint32) pixel[0] << 16) |
                ((guint32) pixel[1] << 8) | pixel[2];
        }
    }
    cairo_surface_mark_dirty(surface);
    gboolean ok = surface_to_result(surface, width, height, result, error);
    cairo_surface_destroy(surface); heif_image_release(image);
    heif_image_handle_release(handle); heif_context_free(context); return ok;
failed:
    if (error != NULL && *error == NULL) g_set_error(error, preview_error(), 6,
        "HEIC/HEIF illisible : %s", heif_error.message);
    if (image != NULL) heif_image_release(image);
    if (handle != NULL) heif_image_handle_release(handle);
    heif_context_free(context); return FALSE;
}
static gboolean load_pdf(const char *path, EvidencePreviewResult *result,
    GError **error)
{
    char *uri = g_filename_to_uri(path, NULL, error);
    if (uri == NULL) return FALSE;
    PopplerDocument *document = poppler_document_new_from_file(uri, NULL, error);
    g_free(uri); if (document == NULL) return FALSE;
    gint pages = poppler_document_get_n_pages(document);
    if (pages < 1) { g_object_unref(document); g_set_error_literal(error,
        preview_error(), 7, "Le PDF ne contient aucune page."); return FALSE; }
    PopplerPage *page = poppler_document_get_page(document, 0);
    double page_width = 0, page_height = 0;
    poppler_page_get_size(page, &page_width, &page_height);
    if (!valid_dimensions((gint) page_width, (gint) page_height, error)) {
        g_object_unref(page); g_object_unref(document); return FALSE;
    }
    double scale = MIN(1.0, MIN(EVIDENCE_PREVIEW_MAX_EDGE / page_width,
        EVIDENCE_PREVIEW_MAX_EDGE / page_height));
    gint width = MAX(1, (gint) (page_width * scale));
    gint height = MAX(1, (gint) (page_height * scale));
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface); cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr); cairo_scale(cr, scale, scale); poppler_page_render(page, cr);
    cairo_destroy(cr); result->item_count = (guint) pages;
    gboolean ok = surface_to_result(surface, width, height, result, error);
    cairo_surface_destroy(surface); g_object_unref(page);
    g_object_unref(document); return ok;
}
static char *header_line(EmlAnalysis *analysis, const char *name,
    const char *label)
{
    const char *value = eml_analysis_get_first_header(analysis, name);
    return g_strdup_printf("%s : %s\n", label,
        value != NULL ? value : "Non renseigné");
}
static gboolean load_email(const char *path, const guint8 *data, gsize size,
    EvidencePreviewResult *result, GError **error)
{
    EmlAnalysis *analysis = eml_analyzer_analyze_file(path, error);
    if (analysis == NULL) return FALSE;
    EmlMimeResult *mime = eml_mime_build_preview(path, NULL, error);
    if (mime == NULL) { eml_analysis_free(analysis); return FALSE; }
    GString *text = g_string_new("APERÇU EML — contenu passif\n\n");
    static const struct { const char *name; const char *label; } headers[] = {
        {"from","From"}, {"to","To"}, {"cc","Cc"}, {"date","Date"},
        {"subject","Subject"}, {"message-id","Message-ID"}};
    for (guint i = 0; i < G_N_ELEMENTS(headers); i++) {
        char *line = header_line(analysis, headers[i].name, headers[i].label);
        g_string_append(text, line); g_free(line);
    }
    (void) data; (void) size;
    g_string_append(text, "\nCorps sélectionné :\n");
    g_string_append(text, mime->body_text != NULL
        ? mime->body_text : "Corps absent.");
    g_string_append_printf(text, "\n\nPièces jointes (%u) :\n",
        mime->attachments->len);
    for (guint i = 0; i < mime->attachments->len; i++) {
        EmlAttachment *attachment = g_ptr_array_index(mime->attachments, i);
        g_string_append_printf(text,
            "• %s — %s — %" G_GSIZE_FORMAT " octets — %s%s%s\n",
            attachment->decoded_filename,
            attachment->content_type,
            attachment->decoded_size,
            attachment->normalized_disposition != NULL
                ? attachment->normalized_disposition : "sans disposition",
            attachment->normalized_content_id != NULL ? " — Content-ID: " : "",
            attachment->normalized_content_id != NULL
                ? attachment->normalized_content_id : "");
    }
    if (mime->warnings->len > 0) {
        g_string_append(text, "\nAvertissements :\n");
        for (guint i = 0; i < mime->warnings->len; i++)
            g_string_append_printf(text, "• %s\n",
                (const char *) g_ptr_array_index(mime->warnings, i));
    }
    result->item_count = mime->attachments->len;
    result->truncated = mime->body_truncated;
    result->text = g_string_free(text, FALSE);
    result->preview_available = TRUE;
    eml_mime_result_free(mime); eml_analysis_free(analysis); return TRUE;
}

EvidencePreviewRequest *evidence_preview_request_new(const char *root,
    const char *identifier, const char *relative_path, const char *sha256,
    const char *mime_type, guint64 generation)
{
    if (root == NULL || !g_uuid_string_is_valid(identifier) ||
        relative_path == NULL || sha256 == NULL) return NULL;
    EvidencePreviewRequest *request = g_new0(EvidencePreviewRequest, 1);
    request->investigation_root_path = g_strdup(root);
    request->evidence_identifier = g_strdup(identifier);
    request->relative_path = g_strdup(relative_path);
    request->expected_sha256 = g_strdup(sha256);
    request->mime_type = g_strdup(mime_type);
    request->request_generation = generation; return request;
}
void evidence_preview_request_free(EvidencePreviewRequest *request)
{
    if (request == NULL) return;
    g_free(request->investigation_root_path); g_free(request->evidence_identifier);
    g_free(request->relative_path); g_free(request->expected_sha256);
    g_free(request->mime_type); g_free(request);
}
void evidence_preview_result_free(EvidencePreviewResult *result)
{
    if (result == NULL) return;
    g_free(result->evidence_identifier); g_free(result->effective_mime_type);
    g_free(result->effective_format); g_free(result->display_name);
    g_free(result->controlled_path); g_free(result->text);
    g_free(result->message); g_clear_pointer(&result->png_bytes, g_bytes_unref);
    g_free(result);
}
gboolean evidence_preview_result_matches(const EvidencePreviewResult *result,
    const char *identifier, guint64 generation)
{ return result != NULL && result->request_generation == generation &&
    g_strcmp0(result->evidence_identifier, identifier) == 0; }

EvidencePreviewResult *evidence_preview_load(
    const EvidencePreviewRequest *request, GCancellable *cancellable,
    GError **error)
{
    if (request == NULL) { g_set_error_literal(error, preview_error(), 1,
        "Aperçu indisponible."); return NULL; }
    EvidenceIntegrityVerificationResult *verification =
        evidence_integrity_verifier_verify(request->investigation_root_path,
            request->relative_path, request->expected_sha256,
            cancellable, error);
    if (verification == NULL) return NULL;
    if (evidence_integrity_verification_result_get_status(verification) !=
        EVIDENCE_INTEGRITY_STATUS_VALID) {
        evidence_integrity_verification_result_free(verification);
        g_set_error_literal(error, preview_error(), 2,
            "Aperçu refusé : l'intégrité de la preuve est invalide.");
        return NULL;
    }
    guint64 size = evidence_integrity_verification_result_get_size_bytes(
        verification);
    char *path = g_build_filename(request->investigation_root_path,
        request->relative_path, NULL);
    EvidencePreviewResult *result = result_new(request, path, size);
    gsize read_limit = (gsize) MIN(size, EVIDENCE_PREVIEW_MAX_TEXT_BYTES);
    guint8 *data = g_malloc(read_limit + 1); gsize read_size = 0;
    GFile *file = g_file_new_for_path(path);
    GFileInputStream *stream = g_file_read(file, cancellable, error);
    if (stream == NULL || !g_input_stream_read_all(G_INPUT_STREAM(stream),
        data, read_limit, &read_size, cancellable, error)) goto failed;
    data[read_size] = '\0'; g_object_unref(stream); stream = NULL;
    PreviewFormat format = detect_format(data, read_size, path,
        request->mime_type);
    if ((format == FORMAT_PNG || format == FORMAT_JPEG ||
         format == FORMAT_HEIF || format == FORMAT_HEIC) &&
        size > EVIDENCE_PREVIEW_MAX_FILE_BYTES) {
        result->kind = EVIDENCE_PREVIEW_KIND_IMAGE;
        result->message = g_strdup(
            "Fichier importable, mais aperçu désactivé en raison de sa taille.");
        goto done;
    }
    gboolean ok = TRUE;
    switch (format) {
    case FORMAT_PNG:
        result->kind = EVIDENCE_PREVIEW_KIND_IMAGE;
        result->effective_mime_type = g_strdup("image/png");
        result->effective_format = g_strdup("PNG");
        ok = load_png(path, result, error); break;
    case FORMAT_JPEG:
        result->kind = EVIDENCE_PREVIEW_KIND_IMAGE;
        result->effective_mime_type = g_strdup("image/jpeg");
        result->effective_format = g_strdup("JPEG");
        ok = load_jpeg(path, result, cancellable, error); break;
    case FORMAT_HEIC: case FORMAT_HEIF:
        result->kind = EVIDENCE_PREVIEW_KIND_IMAGE;
        result->effective_mime_type = g_strdup(format == FORMAT_HEIC
            ? "image/heic" : "image/heif");
        result->effective_format = g_strdup(format == FORMAT_HEIC
            ? "HEIC" : "HEIF");
        ok = load_heif(path, result, error); break;
    case FORMAT_MP4: case FORMAT_MOV:
        result->kind = EVIDENCE_PREVIEW_KIND_VIDEO;
        result->effective_mime_type = g_strdup(format == FORMAT_MOV
            ? "video/quicktime" : "video/mp4");
        result->effective_format = g_strdup(format == FORMAT_MOV
            ? "MOV" : "MP4");
        result->preview_available = TRUE;
        result->message = g_strdup("Vidéo reconnue. Lecture manuelle uniquement.");
        break;
    case FORMAT_TEXT:
        result->kind = EVIDENCE_PREVIEW_KIND_TEXT;
        result->effective_mime_type = g_strdup("text/plain");
        result->effective_format = g_strdup("Texte brut");
        result->text = g_utf8_make_valid((const char *) data, read_size);
        result->truncated = size > read_size; result->preview_available = TRUE;
        break;
    case FORMAT_EML:
        result->kind = EVIDENCE_PREVIEW_KIND_EMAIL;
        result->effective_mime_type = g_strdup("message/rfc822");
        result->effective_format = g_strdup("EML");
        ok = load_email(path, data, read_size, result, error); break;
    case FORMAT_PDF:
        result->kind = EVIDENCE_PREVIEW_KIND_PDF;
        result->effective_mime_type = g_strdup("application/pdf");
        result->effective_format = g_strdup("PDF");
        ok = load_pdf(path, result, error); break;
    default:
        result->message = g_strdup("Format non pris en charge ou contenu invalide.");
        result->effective_format = g_strdup("Inconnu"); break;
    }
    if (!ok) goto failed;
done:
    g_free(data); g_object_unref(file); g_free(path);
    evidence_integrity_verification_result_free(verification); return result;
failed:
    if (stream != NULL) g_object_unref(stream);
    evidence_preview_result_free(result); g_free(data); g_object_unref(file);
    g_free(path); evidence_integrity_verification_result_free(verification);
    return NULL;
}
