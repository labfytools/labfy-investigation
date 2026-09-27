#include "core/research_adapter_dns.h"
#include <string.h>
static gboolean dns_name(const char *s) {
  if (!s || !*s || strlen(s) > 253 || strchr(s, '\\') || strchr(s, '@')) return FALSE;
  for (const unsigned char *p=(const unsigned char *)s; *p; p++)
    if (*p < 0x21 || *p == 0x7f) return FALSE;
  return TRUE;
}
static char *record_string(const ResearchDnsRecord *r) {
  return g_strdup_printf("%s\t%u\t%u\t%s", r->owner, r->ttl,
                         r->preference, r->value);
}
gboolean research_adapter_dns_normalize(const ResearchAction *action,
  ResearchDnsType requested_type, const ResearchDnsFixture *fixture,
  ResearchDnsResult *out, GError **error) {
  if (!action || !dns_name(action->subject) || !fixture || !out ||
      (fixture->record_count > 0 && fixture->records == NULL) ||
      requested_type > RESEARCH_DNS_PTR || fixture->status > RESEARCH_DNS_STATUS_TIMEOUT) {
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Réponse DNS typée invalide."); return FALSE;
  }
  ResearchDnsResult result={.status=fixture->status,
    .records=g_ptr_array_new_with_free_func(g_free)};
  GHashTable *edges=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
  for (gsize i=0;i<fixture->record_count;i++) {
    const ResearchDnsRecord *r=&fixture->records[i];
    if (r->type != requested_type || !dns_name(r->owner) || !r->value || !*r->value) {
      g_hash_table_unref(edges); research_dns_result_clear(&result);
      g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,"Enregistrement DNS incohérent."); return FALSE;
    }
    g_ptr_array_add(result.records,record_string(r));
    if (r->type == RESEARCH_DNS_CNAME) g_hash_table_replace(edges,g_ascii_strdown(r->owner,-1),g_ascii_strdown(r->value,-1));
  }
  if (requested_type == RESEARCH_DNS_CNAME) {
    GHashTableIter iter; gpointer start;
    g_hash_table_iter_init(&iter,edges);
    while (g_hash_table_iter_next(&iter,&start,NULL)) {
      GHashTable *seen=g_hash_table_new(g_str_hash,g_str_equal); const char *cur=start;
      while (cur && g_hash_table_add(seen,(gpointer)cur)) cur=g_hash_table_lookup(edges,cur);
      if (cur) result.cname_cycle=TRUE;
      g_hash_table_unref(seen);
    }
  }
  g_hash_table_unref(edges);
  /* INVARIANT: une cible CNAME est une piste, jamais une propagation de scope. */
  result.next_piste=fixture->status==RESEARCH_DNS_STATUS_OK && result.records->len>0 && !result.cname_cycle;
  *out=result; return TRUE;
}
void research_dns_result_clear(ResearchDnsResult *r) { if (!r) return; g_clear_pointer(&r->records,g_ptr_array_unref); *r=(ResearchDnsResult){0}; }
