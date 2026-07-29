#include "core/identity_field_extractor.h"
typedef struct { char *text; gint x,y,w,h; double confidence; } Word;
static void word_free(gpointer p){Word*w=p;if(w){g_free(w->text);g_free(w);}}
static GPtrArray *parse_tsv(const char *tsv)
{
 GPtrArray*a=g_ptr_array_new_with_free_func(word_free);
 char **lines=g_strsplit(tsv!=NULL?tsv:"","\n",-1);
 for(guint i=1;lines[i]!=NULL;i++){char **c=g_strsplit(lines[i],"\t",12);
  if(g_strv_length(c)>=12&&c[11][0]!='\0'){Word*w=g_new0(Word,1);
   w->x=(gint)g_ascii_strtoll(c[6],NULL,10);w->y=(gint)g_ascii_strtoll(c[7],NULL,10);
   w->w=(gint)g_ascii_strtoll(c[8],NULL,10);w->h=(gint)g_ascii_strtoll(c[9],NULL,10);
   w->confidence=g_ascii_strtod(c[10],NULL);w->text=g_strdup(c[11]);
   g_ptr_array_add(a,w);}g_strfreev(c);}g_strfreev(lines);return a;
}
typedef struct {const char*label;const char*code;} Rule;
static const Rule rules[]={
 {"NOM :","surname"},{"NOM DE NAISSANCE :","birth_name"},
 {"PRÉNOMS :","given_names"},{"PRENOMS :","given_names"},
 {"DATE DE NAISSANCE :","birth_date"},{"LIEU DE NAISSANCE :","birth_place"},
 {"NATIONALITÉ :","nationality"},{"N° DU DOCUMENT :","document_number"},
 {"NUMÉRO DU DOCUMENT :","document_number"},{"DÉLIVRÉ LE :","issue_date"},
 {"EXPIRE LE :","expiry_date"},{"AUTORITÉ :","issuing_authority"},
 {"ADRESSE :","address_as_printed"}};
GPtrArray *identity_field_extractor_extract(const char *text,const char *tsv,
 gint iw,gint ih,GError **error)
{
 (void)error;GPtrArray*out=g_ptr_array_new_with_free_func(
  (GDestroyNotify)identity_field_observation_free);
 GPtrArray*words=parse_tsv(tsv);char**lines=g_strsplit(text!=NULL?text:"","\n",-1);
 guint order=0;
 for(guint i=0;lines[i]!=NULL;i++){char*line=g_strstrip(lines[i]);
  for(guint r=0;r<G_N_ELEMENTS(rules);r++)if(g_str_has_prefix(line,rules[r].label)){
   char*value=g_strdup(line+strlen(rules[r].label));g_strstrip(value);
   if(value[0]!='\0'){IdentitySourceBox box={.page=1,.image_width=iw,.image_height=ih};
    double confidence=-1.0;for(guint w=0;w<words->len;w++){Word*word=g_ptr_array_index(words,w);
     if(g_strstr_len(value,-1,word->text)!=NULL){if(!box.available){box.x=word->x;box.y=word->y;
       box.width=word->w;box.height=word->h;box.available=TRUE;}else{
       gint right=MAX(box.x+box.width,word->x+word->w);gint bottom=MAX(box.y+box.height,word->y+word->h);
       box.x=MIN(box.x,word->x);box.y=MIN(box.y,word->y);box.width=right-box.x;box.height=bottom-box.y;}
       confidence=MAX(confidence,word->confidence);}}
    IdentityFieldObservation*f=identity_field_observation_new(rules[r].code,
      value,confidence,&box,order++);if(f!=NULL)g_ptr_array_add(out,f);}g_free(value);break;}}
 g_strfreev(lines);g_ptr_array_unref(words);return out;
}
