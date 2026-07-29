#include "core/mrz_parser.h"
static gint value(gchar c)
{
 if(c>='0'&&c<='9')return c-'0';
 if(c>='A'&&c<='Z')return c-'A'+10;
 if(c=='<')return 0;
 return -1;
}
gint mrz_parser_check_digit(const char*s){static const gint weights[]={7,3,1};
 gint sum=0;if(s==NULL)return -1;for(gsize i=0;s[i];i++){gint v=value(s[i]);
 if(v<0)return -1;
 sum+=v*weights[i%3];}return sum%10;}
MrzParseResult *mrz_parser_parse(const char*text)
{
 MrzParseResult*r=g_new0(MrzParseResult,1);r->warnings=g_ptr_array_new_with_free_func(g_free);
 char**all=g_strsplit(text!=NULL?text:"","\n",-1);GPtrArray*l=g_ptr_array_new();
 for(guint i=0;all[i]!=NULL;i++){g_strstrip(all[i]);if(strlen(all[i])==44)g_ptr_array_add(l,all[i]);}
 if(l->len>=2){const char*a=g_ptr_array_index(l,0),*b=g_ptr_array_index(l,1);
  r->structure_valid=TRUE;char*num=g_strndup(b,9);r->document_number=g_strdup(num);
  gint expected=value(b[9]);r->check_digits_valid=expected>=0&&mrz_parser_check_digit(num)==expected;
  r->nationality=g_strndup(b+10,3);const char*names=a+5;char**parts=g_strsplit(names,"<<",2);
  r->surname=g_strdup(parts[0]);r->given_names=g_strdup(parts[1]!=NULL?parts[1]:"");
  g_strdelimit(r->surname,"<",' ');g_strdelimit(r->given_names,"<",' ');
  g_strfreev(parts);g_free(num);if(!r->check_digits_valid)g_ptr_array_add(r->warnings,
   g_strdup("Chiffre de contrôle MRZ incohérent ; aucun verdict d’authenticité."));}
 else g_ptr_array_add(r->warnings,g_strdup("Structure MRZ non reconnue."));
 g_ptr_array_unref(l);g_strfreev(all);return r;
}
void mrz_parse_result_free(MrzParseResult*r){if(!r)return;g_free(r->document_number);
 g_free(r->nationality);g_free(r->surname);g_free(r->given_names);
 g_ptr_array_unref(r->warnings);g_free(r);}
