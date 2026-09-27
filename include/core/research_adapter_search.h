#ifndef LABFY_INVESTIGATION_RESEARCH_ADAPTER_SEARCH_H
#define LABFY_INVESTIGATION_RESEARCH_ADAPTER_SEARCH_H
#include "core/research_contracts.h"
G_BEGIN_DECLS
typedef enum { RESEARCH_SEARCH_BRAVE, RESEARCH_SEARCH_SEARXNG } ResearchSearchProvider;
typedef enum { RESEARCH_SEARCH_AVAILABLE, RESEARCH_SEARCH_UNAVAILABLE_CONFIG,
  RESEARCH_SEARCH_UNAVAILABLE_STORAGE_AUTHORIZATION } ResearchSearchAvailability;
typedef struct { ResearchSearchProvider provider; const char *endpoint;
  const char *secret_reference; gboolean storage_authorized; } ResearchSearchConfig;
typedef struct { const char *title; const char *url; const char *snippet; } ResearchSearchHit;
typedef struct { ResearchSearchAvailability availability; GPtrArray *hits;
  gboolean next_piste; } ResearchSearchResult;
ResearchSearchAvailability research_adapter_search_availability(const ResearchSearchConfig *config);
gboolean research_adapter_search_normalize(const ResearchAction *action,
  const ResearchSearchConfig *config,const ResearchSearchHit *hits,gsize hit_count,
  ResearchSearchResult *out,GError **error);
void research_search_result_clear(ResearchSearchResult *result);
G_END_DECLS
#endif
