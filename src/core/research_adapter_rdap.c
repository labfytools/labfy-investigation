#include "core/research_adapter_rdap.h"
#include <string.h>
static gboolean domain_suffix(const char *subject,const char *suffix) {
  gsize a=strlen(subject),b=strlen(suffix); if (!b || b>a) return FALSE;
  return g_ascii_strcasecmp(subject+a-b,suffix)==0 && (a==b || subject[a-b-1]=='.');
}
static gboolean ip_prefix(const char *subject,const char *prefix,guint *bits) {
  char **p=g_strsplit(prefix,"/",2); GInetAddress *a=g_inet_address_new_from_string(subject),*n=g_inet_address_new_from_string(p[0]);
  char *end=NULL; guint64 count=p[1]?g_ascii_strtoull(p[1],&end,10):999;
  gboolean ok=a&&n&&g_inet_address_get_family(a)==g_inet_address_get_family(n);
  guint max=ok&&g_inet_address_get_family(a)==G_SOCKET_FAMILY_IPV4?32:128;
  ok=ok&&p[1]&&end&&*end=='\0'&&count<=max;
  if(ok){const guint8 *x=g_inet_address_to_bytes(a),*y=g_inet_address_to_bytes(n); guint full=count/8,rem=count%8; ok=memcmp(x,y,full)==0 && (!rem || (x[full]>>(8-rem))==(y[full]>>(8-rem)));}
  if (ok)
    *bits = (guint)count;
  g_clear_object(&a);
  g_clear_object(&n);
  g_strfreev(p);
  return ok;
}
gboolean research_adapter_rdap_select(const ResearchAction *action,
  ResearchRdapServiceKind kind,const ResearchRdapBootstrap *b,ResearchRdapResult *out,GError **error){
  if (!action || !action->subject || !b || !out || !b->fetched_at ||
      !b->expires_at || (b->entry_count > 0 && b->entries == NULL)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Bootstrap RDAP invalide.");
    return FALSE;
  }
  const ResearchRdapBootstrapEntry *best=NULL;guint score=0;
  for(gsize i=0;i<b->entry_count;i++){const ResearchRdapBootstrapEntry *e=&b->entries[i];guint s=0;gboolean match=FALSE;
    if(e->kind!=kind||!e->prefix||!e->base_url)continue;
    if(kind==RESEARCH_RDAP_DOMAIN_SERVICE){match=domain_suffix(action->subject,e->prefix);s=(guint)strlen(e->prefix)*8;}else match=ip_prefix(action->subject,e->prefix,&s);
    if(match&&(!best||s>score)){best=e;score=s;}}
  if (!best) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                        "Service RDAP bootstrap introuvable.");
    return FALSE;
  }
  *out = (ResearchRdapResult){g_strdup(best->base_url), score,
      g_strdup(b->fetched_at), g_strdup(b->expires_at), g_strdup(b->etag), TRUE};
  return TRUE;
}
void research_rdap_result_clear(ResearchRdapResult *r){if(!r)return;g_free(r->service_url);g_free(r->cache_fetched_at);g_free(r->cache_expires_at);g_free(r->cache_etag);*r=(ResearchRdapResult){0};}
