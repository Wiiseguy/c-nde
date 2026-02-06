#include <assert.h>

#include "nde.c"

const char *expectedOutput = "[\n"
                             "  {\n"
                             "    \"filename\": \"abc.mp3\",\n"
                             "    \"title\": \"Song Title\",\n"
                             "    \"artist\": \"Artist Name\",\n"
                             "    \"boolTest\": true,\n"
                             "    \"intTest\": 123,\n"
                             "    \"dateTimeTest\": 946684800000,\n"
                             "    \"lengthTest\": 1234567,\n"
                             "    \"longTest\": 9223372036854775808,\n"
                             "    \"redirectorTest\": \"Redirected string\",\n"
                             "    \"binaryTest\": [1, 2, 3, 4, 5],\n"
                             "    \"guidTest\": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],\n"
                             "    \"floatTest\": 3.140000\n"
                             "  },\n"
                             "  {\n"
                             "    \"filename\": \"def.mp3\",\n"
                             "    \"title\": \"UNKNOWN_TYPE: 255 - size: 4\",\n"
                             "    \"artist\": \"Singer person\"\n"
                             "  }\n"
                             "]\n";

int compareStrings(const char *str1, const char *str2)
{
    int line = 1;
    int column = 1;
    char *lastLineStart = (char *)str1;
    while (*str1 && *str2)
    {
        if (*str1 != *str2)
        {
            char *lastLine = (char *)str1 - 1;
            char *endOfLastLine = lastLine;
            while (*endOfLastLine && *endOfLastLine != '\n')
            {
                endOfLastLine++;
            }
            size_t substrLength = (size_t)(endOfLastLine - lastLineStart);
            char lastLineBuffer[256];
            if (substrLength >= sizeof(lastLineBuffer))
            {
                substrLength = sizeof(lastLineBuffer) - 1;
            }
            strncpy(lastLineBuffer, lastLineStart, substrLength);
            lastLineBuffer[substrLength] = '\0';

            printf("Strings differ at line %d, column %d near:\n %s\n", line, column, lastLineBuffer);
            assert(str1 - expectedOutput == str2 - expectedOutput); // Sanity check
            return 0;
        }
        if (*str1 == '\n')
        {
            line++;
            column = 1;
            lastLineStart = (char *)str1 + 1;
        }
        else
        {
            column++;
        }
        str1++;
        str2++;
    }
    return *str1 == *str2;
}

int main(void)
{
    FILE *indexFile;
    const char *indexFilename = "./fixtures/suite.idx";
    indexFile = fopen(indexFilename, "rb");
    if (!indexFile)
    {
        perror("Failed to open file");
        return 1;
    }

    FILE *dataFile;
    const char *dataFilename = "./fixtures/suite.dat";
    dataFile = fopen(dataFilename, "rb");
    if (!dataFile)
    {
        perror("Failed to open file");
        return 1;
    }

    char *defaultOutput = "./output-test.json";

    parse(indexFile, dataFile, defaultOutput);

    fclose(dataFile);
    fclose(indexFile);

    FILE *jsonFile = fopen(defaultOutput, "rb");
    if (!jsonFile)
    {
        perror("Failed to open JSON file for reading");
        return 1;
    }
    fseek(jsonFile, 0, SEEK_END);

    long fileSize = ftell(jsonFile);
    printf("Generated JSON file size: %ld bytes\n", fileSize);
    fseek(jsonFile, 0, SEEK_SET);
    char *jsonContent = (char *)calloc(fileSize + 1, sizeof(char));
    fread(jsonContent, sizeof(char), fileSize, jsonFile);
    fclose(jsonFile);

    if (compareStrings(jsonContent, expectedOutput))
    {
        printf("Test passed: Output matches expected JSON.\n");
    }
    else
    {
        printf("Test failed: Output does not match expected JSON.\n");
        printf("Expected Output:\n%s\n", expectedOutput);
        printf("Actual Output:\n%s\n", jsonContent);
    }
    free(jsonContent);

    return 0;
}