#include "core/research_adapter_page.h"
#include <string.h>
static gboolean append_bounded(GString *s,char c,gsize max){if(s->len>=max)return FALSE;g_string_append_c(s,c);return TRUE;}
gboolean research_adapter_page_normalize(const ResearchAction *a,const ResearchPageFixture *f,gsize max,ResearchPageResult *out,GError **error){
  if(!a||!a->subject||!f||!f->content_type||!f->body||!max||f->body_size>a->max_response_bytes||!g_utf8_validate((const char*)f->body,f->body_size,NULL)){
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Page absente, trop grande ou non UTF-8.");return FALSE;}
  gboolean html=g_ascii_strncasecmp(f->content_type,"text/html",9)==0,text=g_ascii_strncasecmp(f->content_type,"text/plain",10)==0;
  if(!html&&!text){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_SUPPORTED,"Type de page non inerte refusé.");return FALSE;}
  GString *s=g_string_new(NULL);gboolean tag=FALSE,space=FALSE;
  for(gsize i=0;i<f->body_size;i++){char c=(char)f->body[i];if(html&&c=='<'){tag=TRUE;space=TRUE;continue;}if(html&&tag){if(c=='>')tag=FALSE;continue;}if(g_ascii_isspace(c)){if(!space&&s->len&&!append_bounded(s,' ',max))goto large;space=TRUE;}else{if(!append_bounded(s,c,max))goto large;space=FALSE;}}
  g_strstrip(s->str);
  s->len = strlen(s->str);
  gboolean has_text = s->len > 0;
  char *inert_text = g_string_free(s, FALSE);
  *out = (ResearchPageResult){inert_text, html, TRUE, has_text};
  return TRUE;
large:g_string_free(s,TRUE);g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE,"Texte de page hors limite.");return FALSE;
}
void research_page_result_clear(ResearchPageResult *r){if(!r)return;g_free(r->inert_text);*r=(ResearchPageResult){0};}
