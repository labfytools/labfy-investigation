#include "core/research_adapter_search.h"
ResearchSearchAvailability research_adapter_search_availability(const ResearchSearchConfig *c){
  if(!c||!c->endpoint||!*c->endpoint||(c->provider==RESEARCH_SEARCH_BRAVE&&(!c->secret_reference||!*c->secret_reference)))return RESEARCH_SEARCH_UNAVAILABLE_CONFIG;
  if(!c->storage_authorized)return RESEARCH_SEARCH_UNAVAILABLE_STORAGE_AUTHORIZATION;
  return RESEARCH_SEARCH_AVAILABLE;
}
gboolean research_adapter_search_normalize(const ResearchAction *a,const ResearchSearchConfig *c,const ResearchSearchHit *hits,gsize n,ResearchSearchResult *out,GError **error){
  if (!a || !a->subject || !out || (n > 0 && hits == NULL)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Recherche invalide.");
    return FALSE;
  }
  ResearchSearchAvailability available=research_adapter_search_availability(c);
  ResearchSearchResult r={.availability=available,.hits=g_ptr_array_new_with_free_func(g_free)};
  if(available!=RESEARCH_SEARCH_AVAILABLE){*out=r;return TRUE;}
  for(gsize i=0;i<n;i++){
    GUri *uri=hits&&hits[i].url?g_uri_parse(hits[i].url,G_URI_FLAGS_NONE,NULL):NULL;
    const char *scheme = uri != NULL ? g_uri_get_scheme(uri) : NULL;
    if (!uri || (g_strcmp0(scheme, "http") != 0 &&
                 g_strcmp0(scheme, "https") != 0) ||
        !hits[i].title || !hits[i].snippet) {
      if (uri)
        g_uri_unref(uri);
      research_search_result_clear(&r);
      g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                          "Résultat de recherche invalide.");
      return FALSE;
    }
    /* INVARIANT: l'URL reste une piste textuelle, jamais une nouvelle portée. */
    g_ptr_array_add(r.hits,g_strdup_printf("%s\t%s\t%s",hits[i].title,hits[i].url,hits[i].snippet));g_uri_unref(uri);
  }
  r.next_piste=r.hits->len>0;*out=r;return TRUE;
}
void research_search_result_clear(ResearchSearchResult *r){if(!r)return;g_clear_pointer(&r->hits,g_ptr_array_unref);*r=(ResearchSearchResult){0};}
