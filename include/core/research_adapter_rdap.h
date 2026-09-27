#ifndef LABFY_INVESTIGATION_RESEARCH_ADAPTER_RDAP_H
#define LABFY_INVESTIGATION_RESEARCH_ADAPTER_RDAP_H
#include "core/research_contracts.h"
G_BEGIN_DECLS
typedef enum { RESEARCH_RDAP_DOMAIN_SERVICE, RESEARCH_RDAP_IPV4_SERVICE,
  RESEARCH_RDAP_IPV6_SERVICE } ResearchRdapServiceKind;
typedef struct { ResearchRdapServiceKind kind; const char *prefix;
  const char *base_url; } ResearchRdapBootstrapEntry;
typedef struct { const ResearchRdapBootstrapEntry *entries; gsize entry_count;
  const char *fetched_at; const char *expires_at; const char *etag; } ResearchRdapBootstrap;
typedef struct { char *service_url; guint prefix_length; char *cache_fetched_at;
  char *cache_expires_at; char *cache_etag; gboolean next_piste; } ResearchRdapResult;
/** WHY: le bootstrap routé ne vaut jamais autorisation sur l'objet retourné. */
gboolean research_adapter_rdap_select(const ResearchAction *action,
  ResearchRdapServiceKind kind, const ResearchRdapBootstrap *bootstrap,
  ResearchRdapResult *out, GError **error);
void research_rdap_result_clear(ResearchRdapResult *result);
G_END_DECLS
#endif
