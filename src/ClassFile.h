#ifndef CLASSFILE_H
#define CLASSFILE_H

#include <stdint.h>
#include <stdbool.h>

#include "Cursor.h"

typedef enum
{
    CONST_UTF8                 = 1,
    CONST_INT                  = 3,
    CONST_FLOAT                = 4,
    CONST_LONG                 = 5,
    CONST_DOUBLE               = 6,
    CONST_CLASS                = 7,
    CONST_STRING               = 8,
    CONST_FIELD_REF            = 9,
    CONST_METHOD_REF           = 10,
    CONST_INTERFACE_METHOD_REF = 11,
    CONST_NAME_AND_TYPE        = 12,
    CONST_METHOD_HANDLE        = 15,
    CONST_METHOD_TYPE          = 16,
    CONST_INVOKE_DYNAMIC       = 18,
} ConstType;

typedef struct
{
    const uint16_t NameIndex;
} ConstantClass;

typedef struct
{
    const uint16_t Index;
} ConstantString;

typedef struct
{
    const uint16_t ClassIndex;
    const uint16_t NameAndTypeIndex;
} ConstantFieldRef;

typedef struct
{
    const uint16_t ClassIndex;
    const uint16_t NameAndTypeIndex;
} ConstantMethodRef;

typedef struct
{
    const uint16_t ClassIndex;
    const uint16_t NameAndTypeIndex;
} ConstantInterfaceMethodRef;

typedef struct
{
    const uint16_t NameIndex;
    const uint16_t DescriptorIndex;
} ConstantNameAndType;

typedef union
{
    const char* Utf8;
    const int32_t Int;
    const float Float;
    const int64_t Long;
    const double Double;
    const ConstantClass Class;
    const ConstantString String;
    const ConstantFieldRef FieldRef;
    const ConstantMethodRef MethodRef;
    const ConstantInterfaceMethodRef InterfaceMethodRef;
    const ConstantNameAndType NameAndType;
} ConstantAs;

typedef struct
{
    ConstType Type;
    ConstantAs As;
} Constant;

typedef enum
{
    CAF_PUBLIC     = 0x0001,
    CAF_FINAL      = 0x0010,
    CAF_SUPER      = 0x0020,
    CAF_INTERFACE  = 0x0200,
    CAF_ABSTRACT   = 0x0400,
    CAF_SYNTHETIC  = 0x1000,
    CAF_ANNOTATION = 0x2000,
    CAF_ENUM       = 0x4000,
} ClassAccessFlags;

typedef enum
{
    FAF_PUBLIC    = 0x0001,
    FAF_PRIVATE   = 0x0002,
    FAF_PROTECTED = 0x0004,
    FAF_STATIC    = 0x0008,
    FAF_FINAL     = 0x0010,
    FAF_VOLATILE  = 0x0040,
    FAF_TRANSIENT = 0x0080,
    FAF_SYNTHETIC = 0x1000,
    FAF_ENUM      = 0x4000,
} FieldsAccessFlags;

typedef enum
{
    MAF_PUBLIC       = 0x0001,
    MAF_PRIVATE      = 0x0002,
    MAF_PROTECTED    = 0x0004,
    MAF_STATIC       = 0x0008,
    MAF_FINAL        = 0x0010,
    MAF_SYNCHRONIZED = 0x0020,
    MAF_BRIDGE       = 0x0040,
    MAF_VARARGS      = 0x0080,
    MAF_NATIVE       = 0x0100,
    MAF_ABSTRACT     = 0x0400,
    MAF_STRICT       = 0x0800,
    MAF_SYNTHETIC    = 0x1000,
} MethodsAccessFlags;

typedef struct
{
    const uint16_t NameIndex;
    const uint32_t Length;
    const uint8_t* Data;
} AttributeInfo;

typedef struct
{
    const MethodsAccessFlags AccessFlags;
    const uint16_t NameIndex;
    const uint16_t DescriptorIndex;
    const uint16_t AttributesCount;
    const AttributeInfo* Attributes;
} MethodInfo;

typedef struct
{
    const uint32_t Magic;
    const uint16_t Minor;
    const uint16_t Major;
    const uint16_t ConstantPoolCount;
    const Constant* ConstantPool;
    const ClassAccessFlags AccessFlags;
    const uint16_t ThisClass;
    const uint16_t SuperClass;
    // Interfaces NYI
    // Fields NYI
    const uint16_t MethodsCount;
    const MethodInfo* Methods;
    const uint16_t AttributesCount;
    const AttributeInfo* Attributes;
} ClassFile;

bool ReadAttributes(AttributeInfo* attributes, const uint16_t count, Cursor* c, const bool copyData);

ClassFile* ClassFileCreate(const uint8_t* classData, const size_t size);
void ClassFileDestroy(const ClassFile* cf);
const MethodInfo* FindMethodByName(const ClassFile* cf, const char* name);
const AttributeInfo* FindAttributeByName(const ClassFile* cf, const AttributeInfo* attributes, const uint16_t count, const char* name);

#endif //CLASSFILE_H
