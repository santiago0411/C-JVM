#include <assert.h>
#include <stdlib.h>

#include "cursor.h"

#include <string.h>

#define ENSURE_READ(cursor, size) \
    do { \
        if (!((cursor)->ReadPosition + (size) <= (cursor)->Size)) \
            return false; \
    } while(0)

Cursor CursorCreate(const void* data, const size_t size, const bool littleEndian)
{
    return (Cursor) {
        .Data = (uint8_t*)data,
        .Size = size,
        .ReadPosition = 0,
        .LittleEndian = littleEndian
    };
}

bool CursorReadByte(Cursor* cursor, uint8_t* value)
{
    ENSURE_READ(cursor, sizeof(uint8_t));
    *value = cursor->Data[cursor->ReadPosition++];
    return true;
}

bool CursorReadSByte(Cursor* cursor, int8_t* value)
{
    return CursorReadByte(cursor, (uint8_t*)value);
}

bool CursorReadBytesAlloc(Cursor* cursor, uint8_t** buf, const size_t allocSize, const size_t count)
{
    assert(allocSize >= count);
    ENSURE_READ(cursor, count);
    *buf = calloc(allocSize, sizeof(uint8_t));
    CursorReadBytes(cursor, *buf, count);
    return true;
}

bool CursorReadBytes(Cursor* cursor, uint8_t* buf, const size_t count)
{
    ENSURE_READ(cursor, count);

    if (cursor->LittleEndian) {
        memcpy(buf, &cursor->Data[cursor->ReadPosition], count);
        cursor->ReadPosition += count;
        return true;
    }

    for (size_t i = 0; i < count; i++) {
        *(buf + i) = cursor->Data[cursor->ReadPosition + count - 1 - i];
    }

    cursor->ReadPosition += count;
    return true;
}

bool CursorReadUInt16(Cursor* cursor, uint16_t *value)
{
    ENSURE_READ(cursor, sizeof(uint16_t));

    if (cursor->LittleEndian) {
        *value = *(uint16_t*)&cursor->Data[cursor->ReadPosition];
        cursor->ReadPosition += sizeof(uint16_t);
        return true;
    }

    *value = 0;
    *value |= ((uint16_t)cursor->Data[cursor->ReadPosition++] << 8);
    *value |= (uint16_t)cursor->Data[cursor->ReadPosition++];
    return true;
}

bool CursorReadInt16(Cursor* cursor, int16_t* value)
{
    return CursorReadUInt16(cursor, (uint16_t*)value);
}

bool CursorReadUInt32(Cursor* cursor, uint32_t* value)
{
    ENSURE_READ(cursor, sizeof(uint32_t));

    if (cursor->LittleEndian) {
        *value = *(uint32_t*)&cursor->Data[cursor->ReadPosition];
        cursor->ReadPosition += sizeof(uint32_t);
        return true;
    }

    *value = 0;
    *value |= ((uint32_t)cursor->Data[cursor->ReadPosition++] << 24);
    *value |= ((uint32_t)cursor->Data[cursor->ReadPosition++] << 16);
    *value |= ((uint32_t)cursor->Data[cursor->ReadPosition++] << 8);
    *value |= (uint32_t)cursor->Data[cursor->ReadPosition++];
    return true;
}

bool CursorReadInt32(Cursor* cursor, int32_t* value)
{
    return CursorReadUInt32(cursor, (uint32_t*)value);
}

bool CursorReadUInt64(Cursor* cursor, uint64_t* value)
{
    ENSURE_READ(cursor, sizeof(uint64_t));

    if (cursor->LittleEndian) {
        *value = *(uint64_t*)&cursor->Data[cursor->ReadPosition];
        cursor->ReadPosition += sizeof(uint64_t);
        return true;
    }

    *value = 0;
    *value |= ((uint64_t)cursor->Data[cursor->ReadPosition++] << 56);
    *value |= ((uint64_t)cursor->Data[cursor->ReadPosition++] << 48);
    *value |= ((uint64_t)cursor->Data[cursor->ReadPosition++] << 40);
    *value |= ((uint64_t)cursor->Data[cursor->ReadPosition++] << 32);
    *value |= ((uint64_t)cursor->Data[cursor->ReadPosition++] << 24);
    *value |= ((uint64_t)cursor->Data[cursor->ReadPosition++] << 16);
    *value |= ((uint64_t)cursor->Data[cursor->ReadPosition++] << 8);
    *value |= (uint64_t)cursor->Data[cursor->ReadPosition++];
    return true;
}

bool CursorReadInt64(Cursor* cursor, int64_t* value)
{
    return CursorReadUInt64(cursor, (uint64_t*)value);
}

bool CursorReadFloat(Cursor* cursor, float* value)
{
    return CursorReadUInt32(cursor, (uint32_t*)value);
}

bool CursorReadDouble(Cursor* cursor, double* value)
{
    return CursorReadUInt64(cursor, (uint64_t*)value);
}
