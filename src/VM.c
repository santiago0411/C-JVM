#include "VM.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "Cursor.h"
#include "Utils.h"

#define ENSURE_READ(result) \
    do {                    \
        if (!(result)) {    \
            fprintf(stderr, "Error reading from cursor: %s:%d\n", __FILE__, __LINE__); \
            return false;\
        }\
    } while(0)

typedef struct
{
    const uint16_t MaxStack;
    const uint16_t MaxLocals;
    const uint32_t CodeLength;
    const uint8_t* Code;
    const uint16_t TableLength;
    const uint8_t* ExceptionTable; // Not parsed
    const uint16_t AttributesCount;
    const AttributeInfo* Attributes;
} CodeAttribute;

typedef enum
{
    OP_CODE_CONST_M1       = 0x02,
    OP_CODE_CONST_0        = 0x03,
    OP_CODE_CONST_1        = 0x04,
    OP_CODE_CONST_2        = 0x05,
    OP_CODE_CONST_3        = 0x06,
    OP_CODE_CONST_4        = 0x07,
    OP_CODE_CONST_5        = 0x08,
    OP_CODE_BI_PUSH        = 0x10,
    OP_CODE_SI_PUSH        = 0x11,
    OP_CODE_LDC            = 0x12,
    OP_CODE_RETURN         = 0xB1,
    OP_CODE_GET_STATIC     = 0xB2,
    OP_CODE_INVOKE_VIRTUAL = 0xB6,
    OP_CODE_INVOKE_STATIC  = 0xB8,
} OpCode;

typedef enum
{
    TYPE_CLASS_TYPE,
    TYPE_STRING,
    TYPE_BYTE,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_FLOAT,
} ArgumentType;

typedef union
{
    const char* ClassType;
    const char* String;
    uint8_t Byte;
    int16_t Short;
    int32_t Int;
    float Float;
} ArgumentAs;

typedef struct
{
    ArgumentType Type;
    ArgumentAs As;
    uint16_t Frame;
} Argument;

#define STACK_SIZE 4096
static Argument STACK[STACK_SIZE] = {0};
static Argument* STACK_PTR = &STACK[0];
static const Argument*const STACK_START = &STACK[0];
#define STACK_END STACK_START + STACK_SIZE
#define STACK_COUNT (STACK_PTR - STACK_START)

static uint16_t CURRENT_STACK_FRAME = 0;

#define STACK_PUSH_BACK(arg) \
    do { \
        assert(STACK_PTR < STACK_END && "Stack overflow"); \
        (*arg) = STACK_PTR++; \
        (*arg)->Frame = CURRENT_STACK_FRAME; \
    } while(0)

#define STACK_POP(arg) \
    do { \
        assert(STACK_PTR > STACK_START && "Stack is empty"); \
        (*arg) = --STACK_PTR; \
        assert((*arg)->Frame == CURRENT_STACK_FRAME && "Tried to pop an argument that doesn't belong to the current frame"); \
    } while(0)


static uint32_t GetCurrentFrameArgCount()
{
    uint32_t count = 0;
    const Argument* tmp = STACK_PTR;
    while (tmp->Frame == CURRENT_STACK_FRAME || tmp > STACK_START) {
        count++;
        tmp--;
    }
    return count;
}

static bool CodeAttributeCreate(CodeAttribute* ca, Cursor* c)
{
    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&ca->MaxStack));
    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&ca->MaxLocals));
    ENSURE_READ(CursorReadUInt32(c, (uint32_t*)&ca->CodeLength));
    ca->Code = &c->Data[c->ReadPosition];
    c->ReadPosition += ca->CodeLength;

    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&ca->TableLength));
    ca->ExceptionTable = NULL;
    if (ca->TableLength > 0) {
        ca->ExceptionTable = &c->Data[c->ReadPosition];
        c->ReadPosition += ca->TableLength;
    }

    ca->Attributes = NULL;
    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&ca->AttributesCount));
    if (ca->AttributesCount > 0) {
        ca->Attributes = calloc(ca->AttributesCount, sizeof(AttributeInfo));
        assert(ca->Attributes);
        if (!ReadAttributes((AttributeInfo*)ca->Attributes, ca->AttributesCount, c, false)) {
            return false;
        }
    }

    return true;
}

static void CodeAttributeDestroy(const CodeAttribute* ca)
{
    free((void*)ca->Attributes);
    free((void*)ca);
}

static const char* GetNameOfClass(const ClassFile* cf, const uint16_t classIndex)
{
    const Constant* class = &cf->ConstantPool[classIndex - 1];
    assert(class->Type == CONST_CLASS);
    const Constant* className = &cf->ConstantPool[class->As.Class.NameIndex - 1];
    assert(className->Type == CONST_UTF8);
    return className->As.Utf8;
}

static const char* GetNameOfMember(const ClassFile* cf, const uint16_t nameAndTypeIndex)
{
    const Constant* nameAndType = &cf->ConstantPool[nameAndTypeIndex - 1];
    assert(nameAndType->Type == CONST_NAME_AND_TYPE);
    const Constant* memberName = &cf->ConstantPool[nameAndType->As.Class.NameIndex - 1];
    assert(memberName->Type == CONST_UTF8);
    return memberName->As.Utf8;
}

static bool PushIntConst(const int32_t value)
{
    Argument* arg;
    STACK_PUSH_BACK(&arg);
    arg->Type = TYPE_INT;
    arg->As.Int = value;
    return true;
}

static bool BIPush(Cursor* c)
{
    Argument* arg;
    STACK_PUSH_BACK(&arg);
    arg->Type = TYPE_BYTE;
    ENSURE_READ(CursorReadByte(c, &arg->As.Byte));
    return true;
}

static bool SIPush(Cursor* c)
{
    Argument* arg;
    STACK_PUSH_BACK(&arg);
    arg->Type = TYPE_SHORT;
    ENSURE_READ(CursorReadInt16(c, &arg->As.Short));
    return true;
}

static bool LDC(const ClassFile* cf, Cursor* c)
{
    uint8_t index;
    ENSURE_READ(CursorReadByte(c, &index));
    const Constant* constant = &cf->ConstantPool[index - 1];

    Argument* arg;
    STACK_PUSH_BACK(&arg);

    switch (constant->Type) {
        case CONST_INT:
        {
            const Constant* c1 = &cf->ConstantPool[index - 1];
            assert(c1->Type == CONST_INT);
            arg->Type = TYPE_INT;
            arg->As.Int = c1->As.Int;
            break;
        }
        case CONST_FLOAT:
        {
            const Constant* c1 = &cf->ConstantPool[index - 1];
            assert(c1->Type == CONST_FLOAT);
            arg->Type = TYPE_FLOAT;
            arg->As.Float = c1->As.Float;
            break;
        }
        case CONST_STRING:
        {
            const Constant* c1 = &cf->ConstantPool[index - 1];
            assert(c1->Type == CONST_STRING);
            const Constant* c2 = &cf->ConstantPool[c1->As.String.Index - 1];
            assert(c2->Type == CONST_UTF8);
            arg->Type = TYPE_STRING;
            arg->As.String = c2->As.Utf8;
            break;
        }
        default:
        {
            fprintf(stderr, "LDC - Unsupported constant type %d\n", constant->Type);
            return false;
        }
    }

    return true;
}

static bool GetStatic(const ClassFile* cf, Cursor* c)
{
    uint16_t index;
    ENSURE_READ(CursorReadUInt16(c, &index));
    const Constant* constant = &cf->ConstantPool[index - 1];
    assert(constant->Type == CONST_FIELD_REF);

    const char* className = GetNameOfClass(cf, constant->As.FieldRef.ClassIndex);
    const char* memberName = GetNameOfMember(cf, constant->As.FieldRef.NameAndTypeIndex);

    if (!className || !memberName) {
        fprintf(stderr, "GetStatic - ClassName or MemberName not found!!\n");
        return false;
    }

    if (strcmp(className, "java/lang/System") != 0 || strcmp(memberName, "out") != 0) {
        fprintf(stderr, "GetStatic - Unsupported class member %s.%s\n", className, memberName);
        return false;
    }

    Argument* arg;
    STACK_PUSH_BACK(&arg);

    arg->Type = TYPE_CLASS_TYPE;
    arg->As.ClassType = "FakePrintStream";

    return true;
}

static bool InvokeVirtual(const ClassFile* cf, Cursor* c)
{
    uint16_t index;
    ENSURE_READ(CursorReadUInt16(c, &index));
    const Constant* constant = &cf->ConstantPool[index - 1];
    assert(constant->Type == CONST_METHOD_REF);

    const char* className = GetNameOfClass(cf, constant->As.MethodRef.ClassIndex);
    const char* memberName = GetNameOfMember(cf, constant->As.MethodRef.NameAndTypeIndex);

    if (!className || !memberName) {
        fprintf(stderr, "InvokeVirtual - ClassName or MemberName not found!!\n");
        return false;
    }

    if (strcmp(className, "java/io/PrintStream") != 0 || strcmp(memberName, "println") != 0) {
        fprintf(stderr, "InvokeVirtual - Unsupported class member %s.%s\n", className, memberName);
        return false;
    }

    if (STACK_COUNT < 2) {
        fprintf(stderr, "InvokeVirtual - %s.%s expected two arguments but got %zu\n", className, memberName, STACK_COUNT);
        return false;
    }

    Argument* arg1;
    STACK_POP(&arg1);

    Argument* arg0;
    STACK_POP(&arg0);

    assert(arg0->Type == TYPE_CLASS_TYPE);
    if (strcmp(arg0->As.ClassType, "FakePrintStream") != 0) {
        fprintf(stderr, "InvokeVirtual - Unsupported class type %s\n", arg0->As.ClassType);
        assert(false);
    }

    switch (arg1->Type) {
        case TYPE_STRING:
        {
            printf("%s\n", arg1->As.String);
            break;
        }
        case TYPE_BYTE:
        {
            printf("%u\n", arg1->As.Byte);
            break;
        }
        case TYPE_SHORT:
        {
            printf("%d\n", arg1->As.Short);
            break;
        }
        case TYPE_INT:
        {
            printf("%d\n", arg1->As.Int);
            break;
        }
        case TYPE_FLOAT:
        {
            printf("%f\n", arg1->As.Float);
            break;
        }
        default:
        {
            fprintf(stderr, "InvokeVirtual - Expected instruction of type constant but got %d\n", arg1->Type);
            assert(false);
        }
    }

    return true;
}

static bool InvokeStatic(const ClassFile* cf, Cursor* c)
{
    uint16_t index;
    ENSURE_READ(CursorReadUInt16(c, &index));
    const Constant* constant = &cf->ConstantPool[index - 1];
    assert(constant->Type == CONST_METHOD_REF);

    const char* className = GetNameOfClass(cf, constant->As.MethodRef.ClassIndex);

    const Constant* nameAndType = &cf->ConstantPool[constant->As.MethodRef.NameAndTypeIndex - 1];
    assert(nameAndType->Type == CONST_NAME_AND_TYPE);
    const char* methodName = cf->ConstantPool[nameAndType->As.NameAndType.NameIndex - 1].As.Utf8;


    const MethodInfo* method = FindMethodByName(cf, methodName);
    if (!method) {
        fprintf(stderr, "Method %s.%s not found.\n", className, methodName);
        return false;
    }

    assert((method->AccessFlags & MAF_STATIC) > 0 && "Expected static method!");

    const char* descriptor = cf->ConstantPool[nameAndType->As.NameAndType.DescriptorIndex - 1].As.Utf8;
    uint32_t currentFrameArgCount = GetCurrentFrameArgCount();

    if (!ExecuteMethod(cf, method)) {
        fprintf(stderr, "InvokeStatic for %s.%s failed!\n", className, methodName);
        return false;
    }

    return true;
}

bool ExecuteMethod(const ClassFile* cf, const MethodInfo* method)
{
    const Constant* methodNameConst = &cf->ConstantPool[method->NameIndex - 1];
    assert(methodNameConst->Type == CONST_UTF8);
    const char* methodName = methodNameConst->As.Utf8;

    const AttributeInfo* codeAttribute = FindAttributeByName(cf, method->Attributes, method->AttributesCount, "Code");
    if (!codeAttribute) {

        fprintf(stderr, "Failed to find attribute 'Code' inside method '%s'\n", methodName);
        return false;
    }

    Cursor attCursor = CursorCreate(codeAttribute->Data, codeAttribute->Length, false);
    CodeAttribute* ca = calloc(1, sizeof(CodeAttribute));
    assert(ca);

    if (!CodeAttributeCreate(ca, &attCursor)) {
        CodeAttributeDestroy(ca);
        return false;
    }

    CURRENT_STACK_FRAME++;
    Cursor codeCursor = CursorCreate(ca->Code, ca->CodeLength, false);
    bool result = false;
    const void* stackPtrBeforeMethod = STACK_PTR;

    while (codeCursor.ReadPosition < codeCursor.Size) {
        OpCode opCode = 0;
        if (!CursorReadByte(&codeCursor, (uint8_t*)&opCode)) {
            fprintf(stderr, "Failed to read opcode\n");
            break;
        }
        switch (opCode) {
            case OP_CODE_CONST_M1:
            case OP_CODE_CONST_0:
            case OP_CODE_CONST_1:
            case OP_CODE_CONST_2:
            case OP_CODE_CONST_3:
            case OP_CODE_CONST_4:
            case OP_CODE_CONST_5:
            {
                result = PushIntConst((int)opCode - 3);
                break;
            }
            case OP_CODE_BI_PUSH:
            {
                result = BIPush(&codeCursor);
                break;
            }
            case OP_CODE_SI_PUSH:
            {
                result = SIPush(&codeCursor);
                break;
            }
            case OP_CODE_LDC:
            {
                result = LDC(cf, &codeCursor);
                break;
            }
            case OP_CODE_RETURN:
            {
                result = true;
                break;
            }
            case OP_CODE_GET_STATIC:
            {
                result = GetStatic(cf, &codeCursor);
                break;
            }
            case OP_CODE_INVOKE_VIRTUAL:
            {
                result = InvokeVirtual(cf, &codeCursor);
                break;
            }
            case OP_CODE_INVOKE_STATIC:
            {
                result = InvokeStatic(cf, &codeCursor);
                break;
            }
            default:
            {
                fprintf(stderr, "Unsupported OpCode 0x%02x (%d)\n", opCode, opCode);
                result = false;
                break;
            }
        }

        if (!result) {
            fprintf(stderr, "Execution for method '%s' failed!\n", methodName);
            break;
        }
    }

    if (result) {
        // Only assert if the execution of the method was successful
        assert(stackPtrBeforeMethod == STACK_PTR && "Popping stack frame but it still has elements!");
    }

    CURRENT_STACK_FRAME--;
    CodeAttributeDestroy(ca);
    return result;
}