/******************************************************************************
 * @file test_iban_analyzer.c
 * @brief Tests unitaires de l'analyseur IBAN.
 ******************************************************************************/
#include "core/iban_analyzer.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    char *normalized = iban_analyzer_normalize(
        "fr76 3000 6000 0112 3456 7890 189");
    GPtrArray *results = NULL;
    assert(strcmp(normalized, "FR7630006000011234567890189") == 0);
    assert(iban_analyzer_validate(normalized));
    assert(!iban_analyzer_validate("FR7630006000011234567890188"));
    results = iban_analyzer_extract(
        "IBAN : FR76 3000 6000 0112 3456 7890 189\nAutre texte");
    assert(results != NULL && results->len == 1);
    assert(strcmp(g_ptr_array_index(results, 0), normalized) == 0);
    g_ptr_array_unref(results); g_free(normalized);
    puts("IbanAnalyzer : tous les tests sont valides."); return 0;
}
