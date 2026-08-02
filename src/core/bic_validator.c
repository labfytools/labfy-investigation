#include "core/bic_validator.h"
#include <string.h>
char *bic_validator_normalize(const char *bic)
{ if(bic==NULL||bic[0]=='\0')return NULL; char *value=g_ascii_strup(bic,-1);
  if(!bic_validator_validate(value)){g_free(value);return NULL;} return value; }
gboolean bic_validator_validate(const char *bic)
{ return bic_validator_check(bic)==BIC_VALIDATOR_VALID; }
BicValidatorResult bic_validator_check(const char *bic)
{ if(bic==NULL||bic[0]=='\0')return BIC_VALIDATOR_ABSENT;
  gsize n=strlen(bic);if(n!=8&&n!=11)return BIC_VALIDATOR_INVALID;
  for(gsize i=0;i<4;i++)if(!g_ascii_isalpha(bic[i]))return BIC_VALIDATOR_INVALID;
  for(gsize i=4;i<6;i++)if(!g_ascii_isalpha(bic[i]))return BIC_VALIDATOR_INVALID;
  for(gsize i=6;i<n;i++)if(!g_ascii_isalnum(bic[i]))return BIC_VALIDATOR_INVALID;
  return BIC_VALIDATOR_VALID; }
