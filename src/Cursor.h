#ifndef CURSOR_H
#define CURSOR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    const uint8_t*const Data;
    const size_t Size;
    size_t ReadPosition;
    bool LittleEndian;
} Cursor;

Cursor CursorCreate(const void* data, const size_t size, const bool littleEndian);
bool CursorReadByte(Cursor* cursor, uint8_t* value);
bool CursorReadBytesAlloc(Cursor* cursor, uint8_t** buf, const size_t allocSize, const size_t count);
bool CursorReadBytes(Cursor* cursor, uint8_t* buf, const size_t count);
bool CursorReadUInt16(Cursor* cursor, uint16_t* value);
bool CursorReadInt16(Cursor* cursor, int16_t* value);
bool CursorReadUInt32(Cursor* cursor, uint32_t* value);
bool CursorReadInt32(Cursor* cursor, int32_t* value);
bool CursorReadUInt64(Cursor* cursor, uint64_t* value);
bool CursorReadInt64(Cursor* cursor, int64_t* value);
bool CursorReadFloat(Cursor* cursor, float* value);
bool CursorReadDouble(Cursor* cursor, double* value);

#endif //CURSOR_H
