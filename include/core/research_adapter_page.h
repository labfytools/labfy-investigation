#ifndef LABFY_INVESTIGATION_RESEARCH_ADAPTER_PAGE_H
#define LABFY_INVESTIGATION_RESEARCH_ADAPTER_PAGE_H
#include "core/research_contracts.h"
G_BEGIN_DECLS
typedef struct { const char *content_type; const guint8 *body; gsize body_size; } ResearchPageFixture;
typedef struct { char *inert_text; gboolean links_ignored; gboolean attachments_ignored;
  gboolean next_piste; } ResearchPageResult;
/** CONTRACT: produit du texte inerte; aucun script, lien, sous-ressource ou attachment n'est suivi. */
gboolean research_adapter_page_normalize(const ResearchAction *action,
  const ResearchPageFixture *fixture,gsize max_text_bytes,ResearchPageResult *out,GError **error);
void research_page_result_clear(ResearchPageResult *result);
G_END_DECLS
#endif
