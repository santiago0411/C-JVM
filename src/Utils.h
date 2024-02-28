#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <stdint.h>

#define ARRAY_INIT_CAP 2

#define ArrayEnsureCapacity(arr) \
    if ((arr)->Count >= (arr)->Capacity) { \
        (arr)->Capacity = (arr)->Capacity == 0 ? ARRAY_INIT_CAP : (arr)->Capacity * 2; \
        (arr)->Items = realloc((arr)->Items, (arr)->Capacity * sizeof(*(arr)->Items)); \
        assert((arr)->Items && "Out of RAM"); \
    } \

#define ArrayAppend(arr, item) \
    do { \
        ArrayEnsureCapacity((arr)) \
        (arr)->Items[(arr)->Count++] = (item); \
    } while (0)

#define ArrayPushBack(arr, item) \
    do { \
        ArrayEnsureCapacity((arr)) \
        (*(item)) = &(arr)->Items[(arr)->Count]; \
        (arr)->Count++; \
    } while (0)

#define ArrayPop(arr, into) \
    do { \
        if ((arr)->Count > 0) \
            *into = (arr)->Items[--(arr)->Count]; \
    } while (0)

#define ArrayFree(arr) \
    do { \
        free((void*)(arr)->Items); \
        (arr)->Items = NULL; \
        (arr)->Count = 0; \
        (arr)->Capacity = 0; \
    } while (0)


uint8_t* ReadFileToBuffer(const char* filePath, size_t* size);

#endif //UTILS_H
