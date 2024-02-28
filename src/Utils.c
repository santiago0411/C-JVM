#include "Utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t* ReadFileToBuffer(const char* filePath, size_t* size)
{
    FILE* file;
    if (fopen_s(&file, filePath, "rb") != 0) {
        fprintf(stderr, "Failed to open file: %s\n", filePath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    char* buffer = malloc(*size);
    assert(buffer);

    if (fread(buffer, 1, *size, file) != *size) {
        fprintf(stderr, "Failed to read full file to memory.\n");
        fclose(file);
        free(buffer);
        return NULL;
    }

    fclose(file);
    return (uint8_t*)buffer;
}