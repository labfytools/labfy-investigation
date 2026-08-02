#ifndef LABFY_INVESTIGATION_BIC_VALIDATOR_H
#define LABFY_INVESTIGATION_BIC_VALIDATOR_H
#include <glib.h>
G_BEGIN_DECLS
typedef enum
{
    BIC_VALIDATOR_ABSENT,
    BIC_VALIDATOR_VALID,
    BIC_VALIDATOR_INVALID
} BicValidatorResult;
/** Retourne une copie majuscule, ou NULL pour une valeur absente/invalide. */
char *bic_validator_normalize(const char *bic);
/** Distingue explicitement absence, validité syntaxique et invalidité. */
BicValidatorResult bic_validator_check(const char *bic);
/** Validation syntaxique ISO 9362 uniquement ; une absence retourne FALSE. */
gboolean bic_validator_validate(const char *bic);
G_END_DECLS
#endif
