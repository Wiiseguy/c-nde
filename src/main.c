#include <stdint.h>
#include <stdio.h>

#include "nde.c"

int main(int argc, char *argv[])
{
    FILE *indexFile;
    FILE *dataFile;

    char *defaultOutput = "output.json";

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <.idx file> <.dat file>\n", argv[0]);
        return 1;
    }

    const char *indexFilename = argv[1];
    indexFile = fopen(indexFilename, "rb");
    if (!indexFile)
    {
        perror("Failed to open file");
        return 1;
    }

    const char *dataFilename = argv[2];
    dataFile = fopen(dataFilename, "rb");
    if (!dataFile)
    {
        perror("Failed to open file");
        fclose(indexFile);
        return 1;
    }

    parse(indexFile, dataFile, defaultOutput);

    fclose(dataFile);
    fclose(indexFile);

    return 0;
}