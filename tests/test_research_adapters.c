#include "core/research_adapter_cdx.h"
#include "core/research_adapter_dns.h"
#include "core/research_adapter_page.h"
#include "core/research_adapter_rdap.h"
#include "core/research_adapter_search.h"

static ResearchAction action(const char *subject){return (ResearchAction){
  "11111111-1111-4111-8111-111111111111","fixture","specimen",
  "https://example.org/specimen",subject,RESEARCH_CONTACT_THIRD_PARTY,"fixture",1,4096,1000};}

static void test_dns(void){ResearchAction a=action("example.org");
  ResearchDnsRecord records[]={{RESEARCH_DNS_TXT,"example.org","v=specimen",60,0}};
  ResearchDnsFixture f={RESEARCH_DNS_STATUS_OK,records,1};ResearchDnsResult r={0};
  g_assert_true(research_adapter_dns_normalize(&a,RESEARCH_DNS_TXT,&f,&r,NULL));
  g_assert_true(r.next_piste);research_dns_result_clear(&r);
  ResearchDnsRecord cycle[]={{RESEARCH_DNS_CNAME,"a.example","b.example",1,0},{RESEARCH_DNS_CNAME,"b.example","a.example",1,0}};
  f=(ResearchDnsFixture){RESEARCH_DNS_STATUS_OK,cycle,2};
  g_assert_true(research_adapter_dns_normalize(&a,RESEARCH_DNS_CNAME,&f,&r,NULL));
  g_assert_true(r.cname_cycle);g_assert_false(r.next_piste);research_dns_result_clear(&r);
  f=(ResearchDnsFixture){RESEARCH_DNS_STATUS_NXDOMAIN,NULL,0};
  g_assert_true(research_adapter_dns_normalize(&a,RESEARCH_DNS_A,&f,&r,NULL));
  g_assert_false(r.next_piste);research_dns_result_clear(&r);}

static void test_rdap(void){ResearchAction a=action("203.0.113.19");
  ResearchRdapBootstrapEntry entries[]={{RESEARCH_RDAP_IPV4_SERVICE,"0.0.0.0/0","https://rdap.example/all"},{RESEARCH_RDAP_IPV4_SERVICE,"203.0.113.0/24","https://rdap.example/specimen"}};
  ResearchRdapBootstrap b={entries,2,"2026-01-01T00:00:00Z","2026-01-02T00:00:00Z","etag-specimen"};ResearchRdapResult r={0};
  g_assert_true(research_adapter_rdap_select(&a,RESEARCH_RDAP_IPV4_SERVICE,&b,&r,NULL));
  g_assert_cmpuint(r.prefix_length,==,24);g_assert_cmpstr(r.cache_etag,==,"etag-specimen");research_rdap_result_clear(&r);}

static void test_cdx(void){ResearchAction a=action("https://example.org/a");
  ResearchCdxRow rows[]={{"https://example.org/a","20260102030405","digest",200}};
  ResearchCdxFixture f={rows,1,"cursor-2"};ResearchCdxResult r={0};
  g_assert_true(research_adapter_cdx_normalize(&a,RESEARCH_CDX_EXACT_URL,&f,&r,NULL));
  g_assert_cmpstr(r.next_cursor,==,"cursor-2");research_cdx_result_clear(&r);
  rows[0].original_url="https://sub.example.org/a";
  g_assert_false(research_adapter_cdx_normalize(&a,RESEARCH_CDX_EXACT_HOST,&f,&r,NULL));}

static void test_search_and_page(void){ResearchAction a=action("specimen query");ResearchSearchResult s={0};
  ResearchSearchConfig absent={RESEARCH_SEARCH_BRAVE,NULL,NULL,FALSE};
  g_assert_true(research_adapter_search_normalize(&a,&absent,NULL,0,&s,NULL));
  g_assert_cmpint(s.availability,==,RESEARCH_SEARCH_UNAVAILABLE_CONFIG);research_search_result_clear(&s);
  ResearchSearchConfig denied={RESEARCH_SEARCH_SEARXNG,"https://search.example",NULL,FALSE};
  g_assert_cmpint(research_adapter_search_availability(&denied),==,RESEARCH_SEARCH_UNAVAILABLE_STORAGE_AUTHORIZATION);
  ResearchAction page_action=action("example.org");const char html[]="<p>Hello <a href='https://other.example'>world</a></p><script>ignored()</script>";
  ResearchPageFixture f={"text/html",(const guint8*)html,sizeof(html)-1};ResearchPageResult p={0};
  g_assert_true(research_adapter_page_normalize(&page_action,&f,256,&p,NULL));
  g_assert_true(p.links_ignored);g_assert_true(p.attachments_ignored);
  g_assert_null(strstr(p.inert_text,"href"));research_page_result_clear(&p);}

int main(int argc,char **argv){g_test_init(&argc,&argv,NULL);g_test_add_func("/research/adapters/dns",test_dns);g_test_add_func("/research/adapters/rdap",test_rdap);g_test_add_func("/research/adapters/cdx",test_cdx);g_test_add_func("/research/adapters/search-page",test_search_and_page);return g_test_run();}
