#ifndef LABFY_MRZ_PARSER_H
#define LABFY_MRZ_PARSER_H
#include <glib.h>
G_BEGIN_DECLS
typedef struct { gboolean structure_valid; gboolean check_digits_valid;
 char *document_number; char *nationality; char *surname; char *given_names;
 GPtrArray *warnings; } MrzParseResult;
MrzParseResult *mrz_parser_parse(const char *text);
void mrz_parse_result_free(MrzParseResult *result);
gint mrz_parser_check_digit(const char *value);
G_END_DECLS
#endif
