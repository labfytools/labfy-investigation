#ifndef LABFY_INVESTIGATION_RESEARCH_ADAPTER_CDX_H
#define LABFY_INVESTIGATION_RESEARCH_ADAPTER_CDX_H
#include "core/research_contracts.h"
G_BEGIN_DECLS
typedef enum { RESEARCH_CDX_EXACT_URL, RESEARCH_CDX_EXACT_HOST } ResearchCdxMatch;
typedef struct { const char *original_url; const char *timestamp; const char *digest;
  guint status_code; } ResearchCdxRow;
typedef struct { const ResearchCdxRow *rows; gsize row_count; const char *next_cursor; } ResearchCdxFixture;
typedef struct { GPtrArray *captures; char *next_cursor; gboolean next_piste; } ResearchCdxResult;
gboolean research_adapter_cdx_normalize(const ResearchAction *action,
  ResearchCdxMatch match,const ResearchCdxFixture *fixture,ResearchCdxResult *out,GError **error);
void research_cdx_result_clear(ResearchCdxResult *result);
G_END_DECLS
#endif
