/******************************************************************************
 * @file fake_document_tool.c
 * @brief Faux outil documentaire synthétique, sans shell.
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
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

static long argument_long(
    int argc,
    char **argv,
    const char *name,
    long fallback
)
{
    for (int index = 1; index + 1 < argc; index++)
        if (strcmp(argv[index], name) == 0)
            return strtol(argv[index + 1], NULL, 10);
    return fallback;
}

static void write_repeated(FILE *stream, char value, long count)
{
    char block[1024];
    memset(block, value, sizeof(block));
    while (count > 0)
    {
        size_t amount = (size_t) (count > (long) sizeof(block)
            ? (long) sizeof(block) : count);
        if (fwrite(block, 1, amount, stream) != amount)
            return;
        fflush(stream);
        count -= (long) amount;
    }
}

int main(int argc, char **argv)
{
    if (has_argument(argc, argv, "--list-langs"))
    {
        puts("List of available languages in synthetic fixture (2):");
        puts("eng");
        puts("fra");
        return 0;
    }
    if (has_argument(argc, argv, "--emit"))
    {
        long stdout_size = argument_long(
            argc, argv, "--stdout-size", 0);
        long stderr_size = argument_long(
            argc, argv, "--stderr-size", 0);
        long chunks = argument_long(argc, argv, "--chunks", 1);
        int exit_status = (int) argument_long(
            argc, argv, "--exit-status", 0);
        if (chunks < 1)
            chunks = 1;
        for (long index = 0; index < chunks; index++)
        {
            write_repeated(stdout, 'O', stdout_size / chunks);
            write_repeated(stderr, 'E', stderr_size / chunks);
            if (has_argument(argc, argv, "--slow"))
                (void) poll(NULL, 0, 20);
        }
        return exit_status;
    }
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
        if (strstr(argv[argc - 1], "slow") != NULL)
            sleep(2);
        if (strstr(argv[argc - 1], "large") != NULL)
        {
            fputs("[{\"File:MIMEType\":\"image/png\",\"Padding\":\"", stdout);
            write_repeated(stdout, 'X', 8192);
            return 0;
        }
        puts("[{\"File:MIMEType\":\"image/png\","
            "\"File:FileSize\":42,\"EXIF:ImageWidth\":10,"
            "\"EXIF:GPSLatitude\":48.5,\"EXIF:GPSLongitude\":2.2,"
            "\"EXIF:SyntheticBoolean\":true}]");
        return 0;
    }
    if (has_argument(argc, argv, "-enc"))
    {
        const char *path = find_pdf_path(argc, argv);
        if (strstr(path, "slow-text") != NULL)
            sleep(2);
        if (strstr(path, "native") != NULL)
        {
            puts("Texte synthétique suffisamment long pour être considéré "
                 "comme exploitable dans cette fixture.\fDeuxième page.");
        }
        return 0;
    }
    if (has_argument(argc, argv, "-singlefile"))
    {
        const char *pdf_path = find_pdf_path(argc, argv);
        if (strstr(pdf_path, "slow-render") != NULL)
            sleep(2);
        if (strstr(pdf_path, "slow-page-2") != NULL &&
            strcmp(argv[2], "2") == 0)
            sleep(2);
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
        if (strstr(argv[1], "sleep") != NULL ||
            strstr(argv[1], "slow-ocr") != NULL)
            sleep(2);
        if (strstr(argv[1], "large") != NULL)
        {
            write_repeated(stdout, 'T', 8192);
            return 0;
        }
        if (getenv("LABFY_FAKE_IDENTITY_OCR") != NULL &&
            has_argument(argc, argv, "tsv"))
        {
            puts("level\tpage_num\tblock_num\tpar_num\tline_num\tword_num\t"
                 "left\ttop\twidth\theight\tconf\ttext");
            puts("5\t1\t1\t1\t1\t1\t10\t10\t80\t20\t95\tNOM");
            puts("5\t1\t1\t1\t1\t2\t100\t10\t120\t20\t94\tSPECIMEN");
            puts("5\t1\t1\t1\t2\t1\t10\t40\t100\t20\t93\tPRENOM");
            puts("5\t1\t1\t1\t2\t2\t120\t40\t100\t20\t92\tALICE");
            puts("5\t1\t1\t1\t3\t1\t10\t70\t100\t20\t91\tNUMERO");
            puts("5\t1\t1\t1\t3\t2\t120\t70\t100\t20\t90\tABC123");
            puts("5\t1\t1\t1\t4\t1\t10\t100\t110\t20\t89\tNATIONALITE");
            puts("5\t1\t1\t1\t4\t2\t130\t100\t120\t20\t88\tFRANCAIS");
        }
        else if (getenv("LABFY_FAKE_IDENTITY_OCR") != NULL &&
            strstr(argv[1], "page-2") != NULL)
            puts("NOM : PAGEDEUX\nNOM : SPECIMEN\nNOM : PAGE2");
        else if (getenv("LABFY_FAKE_IDENTITY_OCR") != NULL)
            puts("NOM : SPECIMEN\nNOM : ALICE\nNOM : ABC123\n"
                 "NATIONALITÉ : FRANCAIS");
        else if (strstr(argv[1], "page-2") != NULL)
            puts("Texte OCR synthétique page deux.");
        else
            puts("Texte OCR synthétique page une.");
        return 0;
    }
    const char *path = find_pdf_path(argc, argv);
    if (strstr(path, "slow-info") != NULL)
        sleep(2);
    if (strstr(path, "encrypted") != NULL)
        puts("Pages: 2\nEncrypted: yes");
    else
        puts("Pages: 2\nEncrypted: no");
    return 0;
}
