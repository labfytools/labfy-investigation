/******************************************************************************
 * @file local_toolkit_demo.c
 * @brief Lanceur unique SPECIMEN du lot J4.
 ******************************************************************************/
#include "core/local_toolkit_specimen.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *output = NULL;
    for (int index = 1; index + 1 < argc; index++)
        if (strcmp(argv[index], "--output-dir") == 0) output = argv[index + 1];
    if (output == NULL) { fprintf(stderr, "Usage: %s --output-dir DIR\n", argv[0]); return 2; }
    GError *error = NULL;
    if (!local_toolkit_specimen_generate(output, &error)) {
        fprintf(stderr, "Démonstration J4 impossible : %s\n",
            error != NULL ? error->message : "erreur inconnue");
        g_clear_error(&error); return 1;
    }
    printf("Snapshot Toolkit C : %s/core-snapshot.json\n", output);
    return 0;
}
