#include "core/french_bban.h"
#include "core/iban_analyzer.h"
#include <string.h>
FrenchBban *french_bban_derive(const char *iban)
{if(iban==NULL||strlen(iban)!=27||strncmp(iban,"FR",2)!=0||
 !iban_analyzer_validate_canonical(iban))return NULL;
 FrenchBban *b=g_new0(FrenchBban,1);b->bank_code=g_strndup(iban+4,5);
 b->branch_code=g_strndup(iban+9,5);b->account_number=g_strndup(iban+14,11);
 b->rib_key=g_strndup(iban+25,2);return b;}
void french_bban_free(FrenchBban *b)
{if(!b)return;g_free(b->bank_code);g_free(b->branch_code);g_free(b->account_number);g_free(b->rib_key);g_free(b);}
