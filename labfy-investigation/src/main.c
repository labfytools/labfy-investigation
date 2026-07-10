/******************************************************************************
 * @file main.c
 * @brief Point d'entrée de Labfy Investigation.
 ******************************************************************************/

#include "core/application.h"

#include <stdlib.h>

int main(int argc, char **argv)
{
    Application *application = NULL;
    int exit_status = EXIT_FAILURE;

    application = application_new();

    if (application == NULL)
    {
        return EXIT_FAILURE;
    }

    exit_status = application_run(application, argc, argv);

    application_free(application);

    return exit_status;
}
