#include "ClassFile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Cursor.h"

#define ENSURE_READ(result) \
    do {                    \
        if (!(result)) {    \
            fprintf(stderr, "Error reading from cursor: %s:%d\n", __FILE__, __LINE__); \
            ClassFileDestroy(cf); \
            return NULL;\
        }\
    } while(0)


static bool ReadConstantPool(ClassFile* cf, Cursor* c)
{
    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cf->ConstantPoolCount));
    cf->ConstantPool = calloc(cf->ConstantPoolCount, sizeof(Constant));
    assert(cf->ConstantPool);

    for (int i = 0; i < cf->ConstantPoolCount - 1; i++) {
        Constant* cnst = (Constant*)&cf->ConstantPool[i];
        ENSURE_READ(CursorReadByte(c, (uint8_t*)&cnst->Type));

        switch (cnst->Type) {
            case CONST_UTF8:
            {
                uint16_t length;
                ENSURE_READ(CursorReadUInt16(c, &length));
                // Uft8 strings apparently are written in little endian
                c->LittleEndian = true;
                ENSURE_READ(CursorReadBytesAlloc(c, (uint8_t**)&cnst->As.Utf8, length + 1, length));
                c->LittleEndian = false;
                break;
            }
            case CONST_INT:
            {
                ENSURE_READ(CursorReadInt32(c, (int32_t*)&cnst->As.Int));
                break;
            }
            case CONST_FLOAT:
            {
                ENSURE_READ(CursorReadFloat(c, (float*)&cnst->As.Float));
                break;
            }
            case CONST_LONG:
            {
                ENSURE_READ(CursorReadInt64(c, (int64_t*)&cnst->As.Long));
                break;
            }
            case CONST_DOUBLE:
            {
                ENSURE_READ(CursorReadDouble(c, (double*)&cnst->As.Double));
                break;
            }
            case CONST_CLASS:
            {
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.Class.NameIndex));
                break;
            }
            case CONST_STRING:
            {
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.String.Index));
                break;
            }
            case CONST_FIELD_REF:
            {
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.FieldRef.ClassIndex));
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.FieldRef.NameAndTypeIndex));
                break;
            }
            case CONST_METHOD_REF:
            {
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.MethodRef.ClassIndex));
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.MethodRef.NameAndTypeIndex));
                break;
            }
            case CONST_INTERFACE_METHOD_REF:
            {
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.InterfaceMethodRef.ClassIndex));
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.InterfaceMethodRef.NameAndTypeIndex));
                break;
            }
            case CONST_NAME_AND_TYPE:
            {
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.NameAndType.NameIndex));
                ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cnst->As.NameAndType.DescriptorIndex));
                break;
            }
            /*case CONST_METHOD_HANDLE:
                break;
            case CONST_METHOD_TYPE:
                break;
            case CONST_INVOKE_DYNAMIC:
                break;*/
            default:
            {
                fprintf(stderr, "Unsupported ConstType %d\n", cnst->Type);
                assert(false);
            }
        }
    }

    return true;
}

bool ReadAttributes(AttributeInfo* attributes, const uint16_t count, Cursor* c, const bool copyData)
{
    assert(attributes && "Attributes was null");
    for (int i = 0; i < count; i++) {
        AttributeInfo* att = &attributes[i];
        if (!CursorReadUInt16(c, (uint16_t*)&att->NameIndex))
            return false;
        if (!CursorReadUInt32(c, (uint32_t*)&att->Length))
            return false;

        att->Data = NULL;
        if (!copyData) {
            att->Data = &c->Data[c->ReadPosition];
            c->ReadPosition += att->Length;
            return true;
        }

        if (att->Length > 0) {
            c->LittleEndian = true;
            if (!CursorReadBytesAlloc(c, (uint8_t**)&att->Data, att->Length, att->Length)) {
                c->LittleEndian = false;
                return false;
            }
            c->LittleEndian = false;
        }
    }
    return true;
}

static bool ReadMethods(ClassFile* cf, Cursor* c)
{
    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&cf->MethodsCount));
    cf->Methods = calloc(cf->MethodsCount, sizeof(MethodInfo));
    assert(cf->Methods);

    for (int i = 0; i < cf->MethodsCount; i++) {
        MethodInfo* info = (MethodInfo*)&cf->Methods[i];
        ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&info->AccessFlags));
        ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&info->NameIndex));
        ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&info->DescriptorIndex));
        ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&info->AttributesCount));

        if (info->AttributesCount <= 0)
            continue;

        info->Attributes = calloc(info->AttributesCount, sizeof(AttributeInfo));
        assert(info->Attributes);
        if (!ReadAttributes((AttributeInfo*)info->Attributes, info->AttributesCount, c, true)) {
            ClassFileDestroy(cf);
            return false;
        }
    }
    
    return true;
}

ClassFile* ClassFileCreate(const uint8_t* classData, const size_t size)
{
    ClassFile* cf = calloc(1, sizeof(ClassFile));
    assert(cf);

    Cursor cursor = CursorCreate(classData, size, false);

    ENSURE_READ(CursorReadUInt32(&cursor, (uint32_t*)&cf->Magic));
    ENSURE_READ(CursorReadUInt16(&cursor, (uint16_t*)&cf->Minor));
    ENSURE_READ(CursorReadUInt16(&cursor, (uint16_t*)&cf->Major));

    if (!ReadConstantPool(cf, &cursor)) {
        return NULL;
    }

    ENSURE_READ(CursorReadUInt16(&cursor, (uint16_t*)&cf->AccessFlags));
    ENSURE_READ(CursorReadUInt16(&cursor, (uint16_t*)&cf->ThisClass));
    ENSURE_READ(CursorReadUInt16(&cursor, (uint16_t*)&cf->SuperClass));

    uint16_t interfacesCount;
    ENSURE_READ(CursorReadUInt16(&cursor, &interfacesCount));
    assert(interfacesCount == 0 && "Interfaces are not supported!");

    uint16_t fieldsCount;
    ENSURE_READ(CursorReadUInt16(&cursor, &fieldsCount));
    assert(fieldsCount == 0 && "Fields are not supported!");

    if (!ReadMethods(cf, &cursor)) {
        return NULL;
    }

    ENSURE_READ(CursorReadUInt16(&cursor, (uint16_t*)&cf->AttributesCount));
    if (cf->AttributesCount > 0) {
        cf->Attributes = calloc(cf->AttributesCount, sizeof(AttributeInfo));
        assert(cf->Attributes);
        if (!ReadAttributes((AttributeInfo*)cf->Attributes, cf->AttributesCount, &cursor, true)) {
            ClassFileDestroy(cf);
            return false;
        }
    }

    return cf;
}

void ClassFileDestroy(const ClassFile* cf)
{
    if (!cf)
        return;

    if (cf->ConstantPool) {
        for (int i = 0; i < cf->ConstantPoolCount - 1; i++) {
            const Constant* c = &cf->ConstantPool[i];
            if (c && c->Type == CONST_UTF8) {
                free((void*)c->As.Utf8);
            }
        }
        free((void*)cf->ConstantPool);
    }

    if (cf->Methods) {
        for (int i = 0; i < cf->MethodsCount; i++) {
            const MethodInfo* m = &cf->Methods[i];
            if (m) {
                for (int j = 0; j < m->AttributesCount; j++) {
                    const AttributeInfo* a = &m->Attributes[i];
                    if (a) {
                        free((void*)m->Attributes->Data);
                    }
                }
            }
        }
        free((void*)cf->Methods);
    }

    if (cf->Attributes) {
        for (int i = 0; i < cf->AttributesCount; i++) {
            const AttributeInfo* a = &cf->Attributes[i];
            if (a) {
                free((void*)a->Data);
            }
        }
        free((void*)cf->Attributes);
    }

    free((void*)cf);
}

const MethodInfo* FindMethodByName(const ClassFile* cf, const char* name)
{
    for (int i = 0; i < cf->MethodsCount; i++) {
        const MethodInfo* m = &cf->Methods[i];
        const Constant* c = &cf->ConstantPool[m->NameIndex - 1];
        assert(c->Type == CONST_UTF8);
        if (strcmp(c->As.Utf8, name) == 0)
            return m;
    }
    return NULL;
}

const AttributeInfo* FindAttributeByName(const ClassFile* cf, const AttributeInfo* attributes, const uint16_t count, const char* name)
{
    for (uint16_t i = 0; i < count; i++) {
        const AttributeInfo* a = &attributes[i];
        const Constant* c = &cf->ConstantPool[a->NameIndex - 1];
        assert(c->Type == CONST_UTF8);
        if (strcmp(c->As.Utf8, name) == 0)
            return a;
    }
    return NULL;
}
