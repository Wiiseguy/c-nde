#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include <locale.h>

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
        char *strValue;
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
    NDEDataRecordField *fields;
    size_t numFields;
    size_t capacity;
} NDEDataRecordFields;

typedef struct
{
    char id;
    uint8_t type;
    uint32_t size;
    uint32_t next;
    uint32_t prev;
    NDEDataRecordFields fields;
} NDEDataRecord;

typedef struct
{
    NDEDataRecord *records;
    size_t numRecords;
    size_t capacity;
} NDEDataRecords;

typedef struct
{
    uint8_t columnId;
    char *name;
} NDEColumn;

// Global variables
NDEColumn *columns;

// Functions
char readChar(FILE *file)
{
    return (char)getc(file);
}

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

void addRecord(NDEDataRecords *dataRecords, NDEDataRecord record)
{
    if (dataRecords->numRecords >= dataRecords->capacity)
    {
        dataRecords->capacity = (dataRecords->capacity == 0) ? 1 : dataRecords->capacity * 2;
        dataRecords->records = (NDEDataRecord *)realloc(dataRecords->records, dataRecords->capacity * sizeof(NDEDataRecord));
    }
    dataRecords->records[dataRecords->numRecords++] = record;
}

void addField(NDEDataRecordFields *fields, NDEDataRecordField field)
{
    if (fields->numFields >= fields->capacity)
    {
        fields->capacity = (fields->capacity == 0) ? 1 : fields->capacity * 2;
        fields->fields = (NDEDataRecordField *)realloc(fields->fields, fields->capacity * sizeof(NDEDataRecordField));
    }
    fields->fields[fields->numFields++] = field;
}

uint32_t nextIndex(FILE *indexFile)
{
    uint32_t value = readUInt32(indexFile);
    readUInt32(indexFile); // Skip the next 4 bytes
    return value;
}

void readRecord(FILE *file, NDEDataRecord *record)
{
    record->id = readUInt8(file);
    record->type = readUInt8(file);
    record->size = readUInt32(file);
    record->next = readUInt32(file);
    record->prev = readUInt32(file);
}

void readColumns(FILE *dataFile, FILE *indexFile)
{
    uint32_t offset = nextIndex(indexFile);
    fseek(dataFile, offset, SEEK_SET);
    // int tellPos = ftell(dataFile);
    // printf("Column Record Offset: 0x%X, File Position: 0x%X\n", offset, tellPos);

    NDEDataRecord record;
    int numRecords = 0;
    do
    {
        readRecord(dataFile, &record);
        // printf("Column Record ID: %u, Type: %u, Size: %d, Next: 0x%X, Prev: %u\n",
        //        record.id, record.type, record.size, record.next, record.prev);

        uint8_t columnId = readUInt8(dataFile);
        readUInt8(dataFile); // 'unique'
        uint8_t len = readUInt8(dataFile);
        char *name = (char *)calloc(len + 1, sizeof(char));
        fread(name, sizeof(char), len, dataFile);
        columns[(int)record.id].columnId = columnId;
        columns[(int)record.id].name = name;

        fseek(dataFile, record.next, SEEK_SET);
        numRecords++;
    } while (record.next != 0);

    // printf("Defined Columns: %d\n", numRecords);
    for (int i = 0; i < numRecords; i++)
    {
        // printf("Column %u: %s\n", columns[i].columnId, columns[i].name);
    }
}

NDEDataRecord readTableRecord(size_t offset, FILE *dataFile)
{
    fseek(dataFile, offset, SEEK_SET);
    // int tellPos = ftell(dataFile);
    // printf("Data Record Offset: 0x%llX, File Position: 0x%X\n", offset, tellPos);

    NDEDataRecord record = {0};
    do
    {
        readRecord(dataFile, &record);
        // printf("- Data Record ID: %u, Type: %u, Size: %d, Next: 0x%X, Prev: %u\n",
        //        record.id, record.type, record.size, record.next, record.prev);
        if (record.id == -1)
        {
            printf("  Skipping invalid record with ID >= 128\n");
            exit(1);
            return record;
        }

        NDEDataRecordField field = {0};
        field.fieldType = record.type;
        field.fieldSize = record.size;
        field.columnId = record.id;

        switch (record.type)
        {
            // COLUMN (maybe TODO later merge readColumns?)
        // FILENAME
        case FIELD_TYPE_INDEX:
        {
            break;
        }
        case FIELD_TYPE_REDIRECTOR:
        {
            record.next = readUInt32(dataFile);
            break;
        }
        case FIELD_TYPE_STRING:
        case FIELD_TYPE_FILENAME:
        {
            uint16_t len = readUInt16(dataFile);
            char *filename = (char *)calloc(len + 1, sizeof(char));
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
            // printf("  Long Value: %llu\n", field.uint64Value);
            break;
        }
        }

        addField(&record.fields, field);

        fseek(dataFile, record.next, SEEK_SET);
    } while (record.next != 0);
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
    FILE *jsonFile = fopen(filename, "w");
    if (!jsonFile)
    {
        perror("Failed to open JSON file for writing");
        return;
    }
    fprintf(jsonFile, "[\n");
    for (size_t i = 0; i < dataRecords->numRecords; ++i)
    {
        fprintf(jsonFile, "  {\n");
        NDEDataRecord *record = &dataRecords->records[i];
        for (size_t j = 0; j < record->fields.numFields; ++j)
        {
            NDEDataRecordField *field = &record->fields.fields[j];
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
                printf("Binary/GUID Field Size: %u\n", field->fieldSize);
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
                fprintf(jsonFile, "%u", field->uint32Value);
                break;
            case FIELD_TYPE_LONG:
                float jsonLong = (float)field->uint64Value;
                fprintf(jsonFile, "%.0f", jsonLong);
                break;
            default:
                fprintf(jsonFile, "\"(unsupported type)\"");
                break;
            }
            if (j < record->fields.numFields - 1)
            {
                fprintf(jsonFile, ",");
            }
            fprintf(jsonFile, "\n");
        }
        fprintf(jsonFile, "  }");
        if (i < dataRecords->numRecords - 1)
        {
            fprintf(jsonFile, ",");
        }
        fprintf(jsonFile, "\n");
    }

    fprintf(jsonFile, "]\n");
    fclose(jsonFile);
}

int main(int argc, char *argv[])
{
    // setlocale(LC_ALL, "C.UTF-8");
    setlocale(LC_ALL, "en_US.UTF-8");
    SetConsoleOutputCP(CP_UTF8);

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

    columns = (NDEColumn *)malloc(64 * sizeof(NDEColumn)); // Assume max 64 columns

    fseek(indexFile, 8, SEEK_SET); // Skip first 8 bytes (header)
    // uint32_t numRecords = readUInt32(indexFile);

    // Print the read data
    // printf("Number of Records: %u\n", numRecords);

    uint32_t startOfRecords = 16;
    fseek(indexFile, startOfRecords, SEEK_SET);

    // Read the index file
    // for (uint32_t i = 0; i < numRecords; i++)
    // {
    //     uint32_t index = nextIndex(indexFile);
    //     printf("Index %d: %u\n", i + 1, index);
    // }
    fseek(indexFile, startOfRecords, SEEK_SET);

    readColumns(dataFile, indexFile);

    // NDEDataRecord columnRecord = readTableRecord(nextIndex(indexFile), dataFile);
    // addRecord(&dataRecords, columnRecord);
    // writeJson(&dataRecords, defaultOutput);
    // fclose(dataFile);
    // fclose(indexFile);
    // return 0;

    // readColumns(dataFile, indexFile);
    //  readIndexRecord(dataFile, indexFile); // Unused, discarded for now

    // Gobble the index record
    readTableRecord(nextIndex(indexFile), dataFile);

    size_t offset = 0;
    NDEDataRecords dataRecords = {0};
    while ((offset = nextIndex(indexFile)) != 0)
    {
        NDEDataRecord record = readTableRecord(offset, dataFile);
        addRecord(&dataRecords, record);
    }

    // for (size_t i = 0; i < dataRecords.numRecords; i++)
    // {
    //     printf("Processed Record %zu: ID %u, Fields %zu\n",
    //            i + 1,
    //            dataRecords.records[i].id,
    //            dataRecords.records[i].fields.numFields);
    //     for (size_t j = 0; j < dataRecords.records[i].fields.numFields; j++)
    //     {
    //         NDEDataRecordField *field = &dataRecords.records[i].fields.fields[j];
    //         if (field->columnId < 0)
    //         {
    //             printf("  Field %zu: Invalid Column ID: %u, skipping...\n", j + 1, field->columnId);
    //             continue; // Skip invalid column IDs
    //         }
    //         printf("  Field %zu: Column ID: %u, Column Name: \"%s\", Type %u, Size %u\n",
    //                j + 1,
    //                field->columnId,
    //                columns[(int)field->columnId].name,
    //                field->fieldType,
    //                field->fieldSize);

    //         switch (field->fieldType)
    //         {
    //         case FIELD_TYPE_STRING:
    //         case FIELD_TYPE_FILENAME:
    //             if (field->wstrValue)
    //             {
    //                 wprintf(L"    Value: %ls\n", field->wstrValue);
    //             }
    //             break;
    //         case FIELD_TYPE_INTEGER:
    //         case FIELD_TYPE_LENGTH:
    //             printf("    Value: %u\n", field->uint32Value);
    //             break;
    //         case FIELD_TYPE_BOOLEAN:
    //             printf("    Value: %u %s\n", field->boolValue, field->boolValue ? "true" : "false");
    //             break;
    //         case FIELD_TYPE_BINARY:
    //             printf("    Value: (Binary Data of size %u)\n", field->fieldSize);
    //             break;
    //         case FIELD_TYPE_GUID:
    //             printf("    Value: (GUID Data)\n");
    //             break;
    //         case FIELD_TYPE_FLOAT:
    //             printf("    Value: %f\n", field->floatValue);
    //             break;
    //         case FIELD_TYPE_DATETIME:
    //             printf("    Value: %u (DateTime)\n", field->uint32Value);
    //             break;
    //         case FIELD_TYPE_LONG:
    //             printf("    Value: %llu\n", field->uint64Value);
    //             break;
    //         default:
    //             printf("    (Value display not implemented for this type)\n");
    //             break;
    //         }
    //     }
    // }
    printf("Total Records Processed: %zu\n", dataRecords.numRecords);
    writeJson(&dataRecords, defaultOutput);

    fclose(dataFile);
    fclose(indexFile);

    return 0;
}