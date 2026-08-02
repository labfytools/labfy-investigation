#ifndef LABFY_INVESTIGATION_FRENCH_BBAN_H
#define LABFY_INVESTIGATION_FRENCH_BBAN_H
#include <glib.h>
typedef struct { char *bank_code; char *branch_code; char *account_number; char *rib_key; } FrenchBban;
FrenchBban *french_bban_derive(const char *normalized_iban);
void french_bban_free(FrenchBban *bban);
#endif
