#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define dynamic_add(type, array, item)                                                       \
    do                                                                                       \
    {                                                                                        \
        if ((array).size >= (array).capacity)                                                \
        {                                                                                    \
            (array).capacity = ((array).capacity == 0) ? 1 : (array).capacity * 2;           \
            (array).items = (type *)realloc((array).items, (array).capacity * sizeof(type)); \
        }                                                                                    \
        (array).items[(array).size++] = (item);                                              \
    } while (0)

enum FIELD_TYPES
{
    FIELD_TYPE_COLUMN = 0,
    FIELD_TYPE_INDEX = 1,
    FIELD_TYPE_REDIRECTOR = 2,
    FIELD_TYPE_STRING = 3,
    FIELD_TYPE_INTEGER = 4,
    FIELD_TYPE_BOOLEAN = 5,
    FIELD_TYPE_BINARY = 6,
    FIELD_TYPE_GUID = 7,
    FIELD_TYPE_FLOAT = 9,
    FIELD_TYPE_DATETIME = 10,
    FIELD_TYPE_LENGTH = 11,
    FIELD_TYPE_FILENAME = 12, // Assumption, used by filename
    FIELD_TYPE_LONG = 13      // Assumption, used by filesize, which uses 8 bytes
};

// Structs

typedef struct
{
    char columnId;
    uint8_t fieldType;
    uint32_t fieldSize;
    union
    {
        wchar_t *wstrValue;
        uint32_t uint32Value;
        uint8_t boolValue;
        uint8_t *binValue;
        float floatValue;
        uint64_t uint64Value;
    };
} NDEDataRecordField;

typedef struct
{
    NDEDataRecordField *items;
    size_t size;
    size_t capacity;
} NDEDataRecord;

typedef struct
{
    NDEDataRecord *items;
    size_t size;
    size_t capacity;
} NDEDataRecords;

typedef struct
{
    char *name;
} NDEColumn;

// Global variables
NDEColumn *columns;

// Functions

uint8_t readUInt8(FILE *file)
{
    return (uint8_t)getc(file);
}

uint16_t readUInt16(FILE *file)
{
    uint16_t value;
    fread(&value, sizeof(uint16_t), 1, file);
    return value;
}

uint32_t readUInt32(FILE *file)
{
    uint32_t value;
    fread(&value, sizeof(uint32_t), 1, file);
    return value;
}

uint32_t nextIndex(FILE *indexFile)
{
    uint32_t value = readUInt32(indexFile);
    readUInt32(indexFile); // Skip the next 4 bytes
    return value;
}

NDEDataRecord readTableRecord(size_t offset, FILE *dataFile)
{
    fseek(dataFile, offset, SEEK_SET);

    NDEDataRecord record = {0};
    uint32_t next = 0;
    do
    {
        NDEDataRecordField field = {0};
        field.columnId = readUInt8(dataFile);
        field.fieldType = readUInt8(dataFile);
        field.fieldSize = readUInt32(dataFile);
        next = readUInt32(dataFile);
        readUInt32(dataFile); // Gobble prev pointer (unused)

        switch (field.fieldType)
        {
        case FIELD_TYPE_COLUMN:
        {
            readUInt8(dataFile); // Gobble 'unique'
            readUInt8(dataFile); // Gobble column id
            uint8_t len = readUInt8(dataFile);
            char *name = (char *)calloc(len + 1, sizeof(char));
            fread(name, sizeof(char), len, dataFile);
            columns[(int)field.columnId].name = name;
            break;
        }
        case FIELD_TYPE_INDEX:
        {
            break;
        }
        case FIELD_TYPE_REDIRECTOR:
        {
            next = readUInt32(dataFile);
            break;
        }
        case FIELD_TYPE_STRING:
        case FIELD_TYPE_FILENAME:
        {
            uint16_t len = readUInt16(dataFile);
            char *filename = (char *)calloc(len + 2, sizeof(char));
            fread(filename, sizeof(char), len, dataFile);

            // UTF-16
            if (filename[0] == (char)0xff && filename[1] == (char)0xfe)
            {
                wchar_t *wfilename = (wchar_t *)(filename + 2);
                field.wstrValue = wfilename;
            }

            break;
        }
        case FIELD_TYPE_INTEGER:
        case FIELD_TYPE_LENGTH:
        {
            field.uint32Value = readUInt32(dataFile);
            break;
        }
        case FIELD_TYPE_BOOLEAN:
        {
            field.boolValue = readUInt8(dataFile);
            break;
        }
        case FIELD_TYPE_BINARY:
        {
            field.fieldSize = readUInt16(dataFile);
            uint8_t *binData = (uint8_t *)calloc(field.fieldSize, sizeof(uint8_t));
            fread(binData, sizeof(uint8_t), field.fieldSize, dataFile);
            field.binValue = binData;
            break;
        }
        case FIELD_TYPE_GUID:
        {
            uint8_t *guidData = (uint8_t *)calloc(16, sizeof(uint8_t));
            fread(guidData, sizeof(uint8_t), 16, dataFile);
            field.binValue = guidData;
            break;
        }
        case FIELD_TYPE_FLOAT:
        {
            float floatValue;
            fread(&floatValue, sizeof(float), 1, dataFile);
            field.floatValue = floatValue;
            break;
        }
        case FIELD_TYPE_DATETIME:
        {
            uint32_t datetimeValue = readUInt32(dataFile);
            field.uint32Value = datetimeValue;
            break;
        }
        case FIELD_TYPE_LONG:
        {
            uint64_t longValue;
            fread(&longValue, sizeof(uint64_t), 1, dataFile);
            field.uint64Value = longValue;
            break;
        }
        }

        dynamic_add(NDEDataRecordField, record, field);

        fseek(dataFile, next, SEEK_SET);
    } while (next != 0);
    return record;
}

void fprintfEscapedString(FILE *file, const char *str)
{
    fputc('"', file);
    while (*str)
    {
        if (*str == '\\' || *str == '"')
        {
            fputc('\\', file);
        }
        fputc(*str, file);
        str++;
    }
    fputc('"', file);
}

void fprintUtf8String(FILE *file, const wchar_t *wstr)
{
    char *mbStr;
#ifdef _WIN32
    int bufSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    mbStr = (char *)calloc(bufSize, sizeof(char));
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, mbStr, bufSize, NULL, NULL);
#else
    size_t len = wcslen(wstr);
    mbStr = (char *)calloc(len * 4 + 1, sizeof(char));
    wcstombs(mbStr, wstr, len * 4 + 1);
#endif
    fprintfEscapedString(file, mbStr);
    free(mbStr);
}

void writeJson(NDEDataRecords *dataRecords, const char *filename)
{
    FILE *jsonFile = fopen(filename, "wb");
    if (!jsonFile)
    {
        perror("Failed to open JSON file for writing");
        return;
    }
    fprintf(jsonFile, "[\n");
    for (size_t i = 0; i < dataRecords->size; ++i)
    {
        fprintf(jsonFile, "  {\n");
        NDEDataRecord *record = &dataRecords->items[i];
        for (size_t j = 0; j < record->size; ++j)
        {
            NDEDataRecordField *field = &record->items[j];
            if (field->fieldType == FIELD_TYPE_REDIRECTOR || field->columnId < 0)
            {
                continue; // Skip redirector fields in JSON output
            }
            const char *columnName = columns[(int)field->columnId].name;
            fprintf(jsonFile, "    \"%s\": ", columnName);
            switch (field->fieldType)
            {
            case FIELD_TYPE_STRING:
            case FIELD_TYPE_FILENAME:
                if (field->wstrValue)
                {
                    fprintUtf8String(jsonFile, field->wstrValue);
                }
                else
                {
                    fprintf(jsonFile, "null");
                }
                break;
            case FIELD_TYPE_INTEGER:
            case FIELD_TYPE_LENGTH:
                fprintf(jsonFile, "%u", field->uint32Value);
                break;
            case FIELD_TYPE_BOOLEAN:
                fprintf(jsonFile, "%s", field->boolValue ? "true" : "false");
                break;
            case FIELD_TYPE_BINARY:
            case FIELD_TYPE_GUID:
                // Print as array of hex bytes
                fprintf(jsonFile, "[");
                for (uint32_t k = 0; k < field->fieldSize; ++k)
                {
                    fprintf(jsonFile, "%u", field->binValue[k]);
                    if (k < field->fieldSize - 1)
                    {
                        fprintf(jsonFile, ", ");
                    }
                }
                fprintf(jsonFile, "]");
                break;
            case FIELD_TYPE_FLOAT:
                fprintf(jsonFile, "%f", field->floatValue);
                break;
            case FIELD_TYPE_DATETIME:
                fprintf(jsonFile, "%u000", field->uint32Value);
                break;
            case FIELD_TYPE_LONG:
                float jsonLong = (float)field->uint64Value;
                fprintf(jsonFile, "%.0f", jsonLong);
                break;
            default:
                fprintf(jsonFile, "\"UNKNOWN_TYPE: %d - size: %u\"", field->fieldType, field->fieldSize);
                break;
            }
            if (j < record->size - 1)
            {
                fprintf(jsonFile, ",");
            }
            fprintf(jsonFile, "\n");
        }
        fprintf(jsonFile, "  }");
        if (i < dataRecords->size - 1)
        {
            fprintf(jsonFile, ",");
        }
        fprintf(jsonFile, "\n");
    }

    fprintf(jsonFile, "]\n");
    fclose(jsonFile);

    printf("JSON file '%s' written successfully.\n", filename);
}

void parse(FILE *indexFile, FILE *dataFile, char *outputFilename)
{
    columns = (NDEColumn *)malloc(64 * sizeof(NDEColumn)); // Assume max 64 columns

    fseek(indexFile, 8, SEEK_SET); // Skip first 8 bytes (header)

    uint32_t startOfRecords = 16;
    fseek(indexFile, startOfRecords, SEEK_SET);

    // Read the columns
    readTableRecord(nextIndex(indexFile), dataFile);

    // Gobble the index record
    readTableRecord(nextIndex(indexFile), dataFile);

    size_t offset = 0;
    NDEDataRecords dataRecords = {0};
    while ((offset = nextIndex(indexFile)) != 0)
    {
        NDEDataRecord record = readTableRecord(offset, dataFile);
        dynamic_add(NDEDataRecord, dataRecords, record);
    }

    printf("Total Records Processed: %zu\n", dataRecords.size);
    writeJson(&dataRecords, outputFilename);
}
