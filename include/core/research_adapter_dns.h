#ifndef LABFY_INVESTIGATION_RESEARCH_ADAPTER_DNS_H
#define LABFY_INVESTIGATION_RESEARCH_ADAPTER_DNS_H
#include "core/research_contracts.h"
G_BEGIN_DECLS
typedef enum { RESEARCH_DNS_A, RESEARCH_DNS_AAAA, RESEARCH_DNS_CNAME,
  RESEARCH_DNS_MX, RESEARCH_DNS_NS, RESEARCH_DNS_TXT, RESEARCH_DNS_SOA,
  RESEARCH_DNS_PTR } ResearchDnsType;
typedef enum { RESEARCH_DNS_STATUS_OK, RESEARCH_DNS_STATUS_NXDOMAIN,
  RESEARCH_DNS_STATUS_NODATA, RESEARCH_DNS_STATUS_SERVFAIL,
  RESEARCH_DNS_STATUS_TIMEOUT } ResearchDnsStatus;
typedef struct { ResearchDnsType type; const char *owner; const char *value;
  guint ttl; guint preference; } ResearchDnsRecord;
typedef struct { ResearchDnsStatus status; const ResearchDnsRecord *records;
  gsize record_count; } ResearchDnsFixture;
typedef struct { ResearchDnsStatus status; GPtrArray *records;
  gboolean cname_cycle; gboolean next_piste; } ResearchDnsResult;
/** CONTRACT: ANY, AXFR et trace n'existent pas dans le type public. */
gboolean research_adapter_dns_normalize(const ResearchAction *action,
  ResearchDnsType requested_type, const ResearchDnsFixture *fixture,
  ResearchDnsResult *out, GError **error);
void research_dns_result_clear(ResearchDnsResult *result);
G_END_DECLS
#endif
