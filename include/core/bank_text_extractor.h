#ifndef LABFY_INVESTIGATION_BANK_TEXT_EXTRACTOR_H
#define LABFY_INVESTIGATION_BANK_TEXT_EXTRACTOR_H
#include "models/financial_foundation.h"
G_BEGIN_DECLS
/** Extrait des BankCandidate possédés. Les offsets sont des octets UTF-8. */
GPtrArray *bank_text_extractor_extract(const char *text, GError **error);
G_END_DECLS
#endif
