#include "core/research_adapter_cdx.h"
#include <string.h>
static gboolean timestamp(const char *s){if(!s||strlen(s)!=14)return FALSE;for(guint i=0;i<14;i++)if(!g_ascii_isdigit(s[i]))return FALSE;return TRUE;}
static const char *host_of(const char *url,GUri **owned){*owned=g_uri_parse(url,G_URI_FLAGS_NONE,NULL);return *owned?g_uri_get_host(*owned):NULL;}
gboolean research_adapter_cdx_normalize(const ResearchAction *a,ResearchCdxMatch match,const ResearchCdxFixture *f,ResearchCdxResult *out,GError **error){
  if (!a || !a->subject || !f || !out || match > RESEARCH_CDX_EXACT_HOST ||
      (f->row_count > 0 && f->rows == NULL)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Réponse CDX invalide.");
    return FALSE;
  }
  ResearchCdxResult r={.captures=g_ptr_array_new_with_free_func(g_free)};
  GUri *subject_uri=NULL;const char *subject_host=match==RESEARCH_CDX_EXACT_HOST?host_of(a->subject,&subject_uri):NULL;
  if(match==RESEARCH_CDX_EXACT_HOST&&!subject_host)subject_host=a->subject;
  for(gsize i=0;i<f->row_count;i++){const ResearchCdxRow *row=&f->rows[i];GUri *u=NULL;const char *h=host_of(row->original_url,&u);
    gboolean exact=match==RESEARCH_CDX_EXACT_URL?g_strcmp0(row->original_url,a->subject)==0:(h&&g_ascii_strcasecmp(h,subject_host)==0);
    if(!exact||!timestamp(row->timestamp)||!row->digest){if(u)g_uri_unref(u);if(subject_uri)g_uri_unref(subject_uri);research_cdx_result_clear(&r);g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,"Capture CDX hors sujet ou date invalide.");return FALSE;}
    g_ptr_array_add(r.captures,g_strdup_printf("%s\t%s\t%u\t%s",row->timestamp,row->original_url,row->status_code,row->digest));if(u)g_uri_unref(u);}
  if (subject_uri)
    g_uri_unref(subject_uri);
  r.next_cursor = g_strdup(f->next_cursor);
  r.next_piste = r.captures->len > 0;
  *out = r;
  return TRUE;
}
void research_cdx_result_clear(ResearchCdxResult *r){if(!r)return;g_clear_pointer(&r->captures,g_ptr_array_unref);g_free(r->next_cursor);*r=(ResearchCdxResult){0};}
