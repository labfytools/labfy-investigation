#include "core/bank_text_extractor.h"
#include "core/bic_validator.h"
#include "core/iban_analyzer.h"
#include <string.h>

static char *extract_label(const char *block,const char *labels)
{
    char *pattern=g_strdup_printf("(?im)^(?:%s)[ \\t]*:[ \\t]*(.+)$",labels);
    GRegex *regex=g_regex_new(pattern,0,0,NULL);GMatchInfo *match=NULL;char *value=NULL;
    g_regex_match(regex,block,0,&match);if(g_match_info_matches(match))value=g_match_info_fetch(match,1);
    if(value!=NULL)
        g_strstrip(value);
    g_match_info_free(match);g_regex_unref(regex);g_free(pattern);return value;
}
static char *candidate_block(const char *text,gsize start,gsize end)
{
    gsize left=start,right=end,length=strlen(text);
    while(left>0&&start-left<4096U&&!(text[left-1]=='\n'&&left>1&&text[left-2]=='\n'))left--;
    while(right<length&&right-end<4096U&&!(text[right]=='\n'&&right+1<length&&text[right+1]=='\n'))right++;
    return g_strndup(text+left,right-left);
}
static gboolean has_ocr_confusion(const char *raw)
{
    char *compact=iban_analyzer_normalize(raw);gboolean ambiguous=compact!=NULL&&
        strlen(compact)>=4&&(!g_ascii_isdigit(compact[2])||!g_ascii_isdigit(compact[3]));
    g_free(compact);return ambiguous;
}
static void trim_at_first_valid_iban(char **raw,gint *end)
{
    gsize length=strlen(*raw);
    for(gsize size=15;size<=length;size++) {
        unsigned char next=(unsigned char)(*raw)[size];
        if(next!='\0'&&g_ascii_isalnum(next))continue;
        char *prefix=g_strndup(*raw,size);
        if(iban_analyzer_validate_canonical(prefix)) {
            g_free(*raw);*raw=prefix;*end-=((gint)length-(gint)size);return;
        }
        g_free(prefix);
    }
}
static void enrich_candidate(BankCandidate *candidate,const char *text)
{
    char *block=candidate_block(text,candidate->start_offset,candidate->end_offset);
    g_free(candidate->source_text);candidate->source_text=g_strdup(block);
    candidate->declared_holder_raw=extract_label(block,"Titulaire|Holder|Payeur|Bénéficiaire");
    candidate->declared_institution=extract_label(block,"Banque|Bank|Établissement");
    candidate->declared_address=extract_label(block,"Adresse(?: de la banque)?|Bank address");
    candidate->raw_bic=extract_label(block,"BIC|SWIFT");
    candidate->normalized_bic=bic_validator_normalize(candidate->raw_bic);
    if(candidate->raw_bic!=NULL&&candidate->normalized_bic==NULL)
        g_ptr_array_add(candidate->warnings,g_strdup("BIC syntaxiquement invalide."));
    g_free(block);
}
GPtrArray *bank_text_extractor_extract(const char *text,GError **error)
{
    GPtrArray *results=g_ptr_array_new_with_free_func((GDestroyNotify)bank_candidate_free);
    if(text==NULL||text[0]=='\0')return results;
    if(strlen(text)>FINANCIAL_TEXT_MAX_BYTES){g_set_error_literal(error,
      g_quark_from_static_string("bank-text-extractor-error"),1,"Le texte bancaire dépasse la limite autorisée.");return results;}
    GRegex *regex=g_regex_new("(?i)\\b[A-Z]{2}[0-9OIl]{2}(?:[ \\t\\r\\n-]*[A-Z0-9OIl]){3,30}\\b",
        G_REGEX_OPTIMIZE,0,NULL);GMatchInfo *matches=NULL;g_regex_match(regex,text,0,&matches);
    while(g_match_info_matches(matches)&&results->len<FINANCIAL_CANDIDATE_MAX_RESULTS) {
        gint start=0,end=0;g_match_info_fetch_pos(matches,0,&start,&end);
        char *raw=g_match_info_fetch(matches,0);trim_at_first_valid_iban(&raw,&end);BankCandidate *candidate=
            bank_candidate_new(text,raw,(gsize)start,(gsize)end,NULL);
        candidate->normalized_iban=iban_analyzer_normalize_canonical(raw);
        if(has_ocr_confusion(raw)) {
            candidate->validation=BANK_VALIDATION_AMBIGUOUS;
            g_clear_pointer(&candidate->normalized_iban,g_free);
            g_ptr_array_add(candidate->warnings,g_strdup("Confusion OCR possible, sans correction automatique."));
        } else if(candidate->normalized_iban==NULL)
            candidate->validation=BANK_VALIDATION_UNVERIFIABLE;
        else {
            IbanAnalyzerResult validation=iban_analyzer_validate_canonical_result(
                candidate->normalized_iban);
            if(validation==IBAN_ANALYZER_RESULT_VALID)
                candidate->validation=BANK_VALIDATION_VALID;
            else if(validation==IBAN_ANALYZER_RESULT_UNSUPPORTED_COUNTRY) {
                candidate->validation=BANK_VALIDATION_UNVERIFIABLE;
                g_ptr_array_add(candidate->warnings,g_strdup(
                    "Pays IBAN non pris en charge."));
            } else candidate->validation=BANK_VALIDATION_INVALID;
        }
        enrich_candidate(candidate,text);g_ptr_array_add(results,candidate);g_free(raw);
        if(!g_match_info_next(matches,NULL))break;
    }
    g_match_info_free(matches);g_regex_unref(regex);
    regex=g_regex_new("(?im)^IBAN[ \\t]*:[ \\t]*([^\\r\\n]{8,80})",0,0,NULL);
    g_regex_match(regex,text,0,&matches);
    while(g_match_info_matches(matches)&&results->len<FINANCIAL_CANDIDATE_MAX_RESULTS) {
        gint start=0,end=0;char *raw=g_match_info_fetch(matches,1);
        g_match_info_fetch_pos(matches,1,&start,&end);gboolean non_ascii=FALSE;
        for(const unsigned char *p=(const unsigned char *)raw;*p;p++)if(*p>=128)non_ascii=TRUE;
        if(non_ascii) {
            BankCandidate *candidate=bank_candidate_new(text,raw,(gsize)start,(gsize)end,NULL);
            candidate->validation=BANK_VALIDATION_AMBIGUOUS;
            g_ptr_array_add(candidate->warnings,g_strdup("Caractère Unicode non normalisé."));
            enrich_candidate(candidate,text);g_ptr_array_add(results,candidate);
        }
        g_free(raw);if(!g_match_info_next(matches,NULL))break;
    }
    g_match_info_free(matches);g_regex_unref(regex);return results;
}
