/******************************************************************************
 * @file fake_document_tool.c
 * @brief Faux outil documentaire synthétique, sans shell.
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int has_argument(int argc, char **argv, const char *value)
{
    for (int index = 1; index < argc; index++)
        if (strcmp(argv[index], value) == 0)
            return 1;
    return 0;
}

static const char *find_pdf_path(int argc, char **argv)
{
    for (int index = 1; index < argc; index++)
        if (strstr(argv[index], ".pdf") != NULL)
            return argv[index];
    return "";
}

int main(int argc, char **argv)
{
    if (has_argument(argc, argv, "-ver"))
    {
        puts("13.00");
        return 0;
    }
    if (has_argument(argc, argv, "--version"))
    {
        puts("tesseract 5.0.0-synthetic");
        return 0;
    }
    if (has_argument(argc, argv, "-j"))
    {
        puts("[{\"File:MIMEType\":\"image/png\","
            "\"File:FileSize\":42,\"EXIF:ImageWidth\":10,"
            "\"EXIF:GPSLatitude\":48.5,\"EXIF:GPSLongitude\":2.2,"
            "\"EXIF:SyntheticBoolean\":true}]");
        return 0;
    }
    if (has_argument(argc, argv, "-enc"))
    {
        const char *path = find_pdf_path(argc, argv);
        if (strstr(path, "native") != NULL)
        {
            puts("Texte synthétique suffisamment long pour être considéré "
                 "comme exploitable dans cette fixture.\fDeuxième page.");
        }
        return 0;
    }
    if (has_argument(argc, argv, "-singlefile"))
    {
        const char *prefix = argv[argc - 1];
        char path[4096];
        if (snprintf(path, sizeof(path), "%s.png", prefix) < 0)
            return 2;
        FILE *file = fopen(path, "wb");
        if (file == NULL)
            return 3;
        fputs("synthetic-image", file);
        fclose(file);
        return 0;
    }
    if (has_argument(argc, argv, "stdout"))
    {
        if (strstr(argv[1], "sleep") != NULL)
            sleep(2);
        if (strstr(argv[1], "page-2") != NULL)
            puts("Texte OCR synthétique page deux.");
        else
            puts("Texte OCR synthétique page une.");
        return 0;
    }
    const char *path = find_pdf_path(argc, argv);
    if (strstr(path, "encrypted") != NULL)
        puts("Pages: 2\nEncrypted: yes");
    else
        puts("Pages: 2\nEncrypted: no");
    return 0;
}
