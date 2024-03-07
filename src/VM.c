#include "VM.h"

#include <stdio.h>
#include <stdlib.h>
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
    OP_CODE_I_LOAD         = 0x15,
    OP_CODE_I_LOAD_0       = 0x1A,
    OP_CODE_I_LOAD_1       = 0x1B,
    OP_CODE_I_LOAD_2       = 0x1C,
    OP_CODE_I_LOAD_3       = 0x1D,
    OP_CODE_I_STORE        = 0x36,
    OP_CODE_I_STORE_0      = 0x3B,
    OP_CODE_I_STORE_1      = 0x3C,
    OP_CODE_I_STORE_2      = 0x3D,
    OP_CODE_I_STORE_3      = 0x3E,
    OP_CODE_I_ADD          = 0x60,
    OP_CODE_I_INC          = 0x84,
    OP_CODE_I_CMP_EQ       = 0x9F,
    OP_CODE_I_CMP_NE       = 0xA0,
    OP_CODE_I_CMP_LT       = 0xA1,
    OP_CODE_I_CMP_GE       = 0xA2,
    OP_CODE_I_CMP_GT       = 0xA3,
    OP_CODE_I_CMP_LE       = 0xA4,
    OP_CODE_GOTO           = 0xA7,
    OP_CODE_I_RETURN       = 0xAC,
    OP_CODE_RETURN         = 0xB1,
    OP_CODE_GET_STATIC     = 0xB2,
    OP_CODE_INVOKE_VIRTUAL = 0xB6,
    OP_CODE_INVOKE_STATIC  = 0xB8,
} OpCode;

typedef enum
{
    TYPE_VOID,
    TYPE_CLASS_TYPE,
    TYPE_STRING,
    TYPE_BYTE,
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_FLOAT,
} ArgumentType;

typedef union
{
    const char* ClassType;
    const char* String;
    uint8_t Byte;
    char Char;
    bool Bool;
    int16_t Short;
    int32_t Int;
    float Float;
} ArgumentAs;

typedef struct
{
    ArgumentType Type;
    ArgumentAs As;
} Argument;

// If a method has more than 10 you deserve the crash lol
#define METHOD_MAX_PARAMS 10

typedef struct
{
    uint8_t ParametersCount;
    ArgumentType ParameterTypes[METHOD_MAX_PARAMS];
    ArgumentType MethodReturnType;
} Descriptor;

typedef struct
{
    uint16_t StackSize;
    Argument* Stack;
    Argument* StackStart;

    // (DOCS:) A single local variable can hold a value of type boolean, byte, char, short, int, float, reference, or returnAddress.
    // A pair of local variables can hold a value of type long or double.
    uint16_t LocalsSize;
    uint32_t* Locals;
} Frame;

static Frame* CURRENT_FRAME = NULL;

#define ALLOC_NEW_FRAME(ca) \
    do { \
        CURRENT_FRAME = malloc(sizeof(Frame)); \
        assert(CURRENT_FRAME); \
        CURRENT_FRAME->StackSize = (ca)->MaxStack; \
        CURRENT_FRAME->Stack = calloc((ca)->MaxStack, sizeof(Argument)); \
        assert(CURRENT_FRAME->Stack); \
        CURRENT_FRAME->StackStart = CURRENT_FRAME->Stack; \
        CURRENT_FRAME->LocalsSize = (ca)->MaxLocals; \
        if (CURRENT_FRAME->LocalsSize > 0) { \
            CURRENT_FRAME->Locals = calloc((ca)->MaxLocals, sizeof(uint32_t)); \
            assert(CURRENT_FRAME->Locals); \
        } \
    } while(0)

#define FREE_CURRENT_FRAME() \
    do { \
        assert(CURRENT_FRAME); \
        free((void*)CURRENT_FRAME->Stack); \
        free((void*)CURRENT_FRAME->Locals); \
        CURRENT_FRAME = NULL; \
    } while(0)

#define STACK_PUSH_BACK(arg) \
    do { \
        assert(CURRENT_FRAME && "CURRENT_FRAME WAS NULL"); \
        assert(CURRENT_FRAME->Stack < CURRENT_FRAME->StackStart + CURRENT_FRAME->StackSize && "Stack overflow"); \
        (*arg) = CURRENT_FRAME->Stack++; \
    } while(0)

#define STACK_POP(arg) \
    do { \
        assert(CURRENT_FRAME && "CURRENT_FRAME WAS NULL"); \
        assert(CURRENT_FRAME->Stack > CURRENT_FRAME->StackStart && "Stack frame is empty"); \
        (*arg) = --CURRENT_FRAME->Stack; \
    } while(0)

#define STACK_COUNT (CURRENT_FRAME->Stack - CURRENT_FRAME->StackStart)

static bool ExecuteCode(const ClassFile* cf, const CodeAttribute* ca);

static bool CodeAttributeCreate(CodeAttribute* ca, Cursor* c)
{
    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&ca->MaxStack));
    ENSURE_READ(CursorReadUInt16(c, (uint16_t*)&ca->MaxLocals));
    ENSURE_READ(CursorReadUInt32(c, (uint32_t*)&ca->CodeLength));
    // This cursor points to ClassFile data so we don't need to copy it, we can just take the pointer to it
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

static CodeAttribute* CreateCodeAttributeFromMethod(const ClassFile* cf, const MethodInfo* method)
{
    const Constant* methodNameConst = &cf->ConstantPool[method->NameIndex - 1];
    assert(methodNameConst->Type == CONST_UTF8);
    const char* methodName = methodNameConst->As.Utf8;

    const AttributeInfo* codeAttInfo = FindAttributeByName(cf, method->Attributes, method->AttributesCount, "Code");
    if (!codeAttInfo) {
        fprintf(stderr, "Failed to find attribute 'Code' inside method '%s'\n", methodName);
        return NULL;
    }

    Cursor attCursor = CursorCreate(codeAttInfo->Data, codeAttInfo->Length, false);
    CodeAttribute* codeAtt = calloc(1, sizeof(CodeAttribute));
    assert(codeAtt);

    if (!CodeAttributeCreate(codeAtt, &attCursor)) {
        CodeAttributeDestroy(codeAtt);
        return NULL;
    }

    return codeAtt;
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

static ArgumentType GetTypeFromDescriptorChar(const char c)
{
    switch (c) {
        case 'B':
            return TYPE_BYTE;
        case 'C':
            return TYPE_CHAR;
        case 'F':
            return TYPE_FLOAT;
        case 'I':
            return TYPE_INT;
        case 'S':
            return TYPE_SHORT;
        case 'Z':
            return TYPE_BOOL;
        case 'V':
            return TYPE_VOID;
        default:
            fprintf(stderr, "Unsupported argument type %c\n", c);
            assert(false);
    }
}

static void ParseDescriptorStr(const char* descStr, Descriptor* desc)
{
    assert(*descStr == '(' && "Descriptor should start with '('");

    // TODO: this doesn't support objects or arrays
    while (*++descStr && *descStr != ')') {
        desc->ParameterTypes[desc->ParametersCount] = GetTypeFromDescriptorChar(*descStr);
        assert(desc->ParameterTypes[desc->ParametersCount] != TYPE_VOID && "Method parameter can't possibly be of type void!");
        desc->ParametersCount++;
        assert(desc->ParametersCount <= METHOD_MAX_PARAMS && "Refactor your garbage code!");
    }

    assert(descStr);
    // At this point descriptor points to ')'
    desc->MethodReturnType = GetTypeFromDescriptorChar(*++descStr);
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

static bool LoadInt(const uint8_t index)
{
    Argument* arg;
    STACK_PUSH_BACK(&arg);
    arg->Type = TYPE_INT;
    arg->As.Int = (int32_t)CURRENT_FRAME->Locals[index];
    return true;
}

static bool IntStore(const uint8_t index)
{
    Argument* arg;
    STACK_POP(&arg);
    assert(arg->Type == TYPE_INT);
    CURRENT_FRAME->Locals[index] = (uint32_t)arg->As.Int;
    return true;
}

static bool IntAdd()
{
    Argument *val1, *val2;
    STACK_POP(&val2);
    STACK_POP(&val1);

    assert(val1->Type == TYPE_INT);
    assert(val2->Type == TYPE_INT);

    Argument* result;
    STACK_PUSH_BACK(&result);

    result->Type = TYPE_INT;
    result->As.Int = val1->As.Int + val2->As.Int;

    return true;
}

static bool IntInc(Cursor* c)
{
    uint8_t index;
    ENSURE_READ(CursorReadByte(c, &index));

    int8_t increase;
    ENSURE_READ(CursorReadSByte(c, &increase));

    CURRENT_FRAME->Locals[index] += (int32_t)increase;

    return true;
}

static bool IntCompare(Cursor* c, const OpCode comparison)
{
    uint16_t branchOffset;
    ENSURE_READ(CursorReadUInt16(c, &branchOffset));

    // -3 because this whole opcode should be rewinded.
    // That means, rewinding 2 bytes for the offset and 1 byte for the actual opcode
    // (DOCS:) Execution then proceeds at that offset from the address of the opcode of this if_icmp<cond>
    branchOffset -= 3;

    Argument *val1, *val2;
    STACK_POP(&val2);
    STACK_POP(&val1);

    assert(val1->Type == TYPE_INT);
    assert(val2->Type == TYPE_INT);

    switch (comparison) {
        case OP_CODE_I_CMP_GE:
        {
            if (val1->As.Int >= val2->As.Int) {
                c->ReadPosition += branchOffset;
            }
            return true;
        }
        default:
        {
            fprintf(stderr, "IntCompare - Invalid int comparison OpCode %x (%d)", comparison, comparison);
            return false;
        }
    }
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

    const char* descriptorStr = cf->ConstantPool[nameAndType->As.NameAndType.DescriptorIndex - 1].As.Utf8;
    Descriptor descriptor = {0};
    ParseDescriptorStr(descriptorStr, &descriptor);

    assert(STACK_COUNT >= descriptor.ParametersCount);
#if defined(APP_DEBUG)
    for (uint8_t i = 0; i < descriptor.ParametersCount; i++) {
        const Argument* arg = &CURRENT_FRAME->Stack[-i - 1];
        assert(arg->Type == descriptor.ParameterTypes[i]);
    }
#endif

    const CodeAttribute* codeAttribute = CreateCodeAttributeFromMethod(cf, method);
    if (!codeAttribute) {
        return false;
    }

    Frame* previousFrame = CURRENT_FRAME;
    ALLOC_NEW_FRAME(codeAttribute);

    // Pop arguments from the previous frame's stack and copy them to the new frame's locals
    for (uint8_t i = 0; i < descriptor.ParametersCount; i++) {
        const Argument* arg = --previousFrame->Stack;
        uint32_t* local = &CURRENT_FRAME->Locals[i];

        switch (arg->Type) {
            case TYPE_CLASS_TYPE:
            {
                assert(false && "COPYING CLASS_TYPE ARG NYI!");
                // break;
            }
            case TYPE_STRING:
            {
                assert(false && "COPY NATIVE STRINGS NYI!");
                // break;
            }
            case TYPE_BYTE:
            {
                *(uint8_t*)local = arg->As.Byte;
                break;
            }
            case TYPE_SHORT:
            {
                *(uint16_t*)local = (uint16_t)arg->As.Short;
                break;
            }
            case TYPE_INT:
            {
                *local = (uint32_t)arg->As.Int;
                break;
            }
            case TYPE_FLOAT:
            {
                *(float*)local = arg->As.Float;
                break;
            }
            default:
            {
                assert(false && "Invalid Type");
            }
        }
    }

    bool result = true;
    if (!ExecuteCode(cf, codeAttribute)) {
        fprintf(stderr, "InvokeStatic for %s.%s failed!\n", className, methodName);
        result = false;
    }

    if (result && descriptor.MethodReturnType != TYPE_VOID) {
        Argument* arg;
        STACK_POP(&arg);
        assert(arg->Type == descriptor.MethodReturnType);
        *previousFrame->Stack++ = *arg;
    }

    FREE_CURRENT_FRAME();
    CURRENT_FRAME = previousFrame;
    return result;
}

static bool ExecuteCode(const ClassFile* cf, const CodeAttribute* ca)
{
    Cursor codeCursor = CursorCreate(ca->Code, ca->CodeLength, false);
    bool result = false;

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
            case OP_CODE_I_LOAD:
            {
                uint8_t index;
                ENSURE_READ(CursorReadByte(&codeCursor, &index));
                result = LoadInt(index);
                break;
            }
            case OP_CODE_I_LOAD_0:
            case OP_CODE_I_LOAD_1:
            case OP_CODE_I_LOAD_2:
            case OP_CODE_I_LOAD_3:
            {
                result = LoadInt((int)opCode - 26);
                break;
            }
            case OP_CODE_I_STORE:
            {
                uint8_t index;
                ENSURE_READ(CursorReadByte(&codeCursor, &index));
                result = IntStore(index);
                break;
            }
            case OP_CODE_I_STORE_0:
            case OP_CODE_I_STORE_1:
            case OP_CODE_I_STORE_2:
            case OP_CODE_I_STORE_3:
            {
                result = IntStore((int)opCode - 59);
                break;
            }
            case OP_CODE_I_ADD:
            {
                result = IntAdd();
                break;
            }
            case OP_CODE_I_INC:
            {
                result = IntInc(&codeCursor);
                break;
            }
            case OP_CODE_I_CMP_GE:
            {
                result = IntCompare(&codeCursor, opCode);
                break;
            }
            case OP_CODE_GOTO:
            {
                int16_t branchOffSet; // May be negative
                ENSURE_READ(CursorReadInt16(&codeCursor, &branchOffSet));

                // -3 because this whole opcode should be rewinded.
                // That means, rewinding 2 bytes for the offset and 1 byte for the actual opcode
                // (DOCS:) Execution proceeds at that offset from the address of the opcode of this goto instruction.
                codeCursor.ReadPosition += (branchOffSet - 3);
                result = true;
                break;
            }
            case OP_CODE_I_RETURN:
            {
                assert(CURRENT_FRAME->Stack->Type == TYPE_INT);
                result = true;
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
                assert(false);
            }
        }

        if (!result) {
            break;
        }
    }

    return result;
}

bool ExecuteMethod(const ClassFile* cf, const MethodInfo* method)
{
    const CodeAttribute* ca = CreateCodeAttributeFromMethod(cf, method);
    if (!ca) {
        return false;
    }

    ALLOC_NEW_FRAME(ca);
    const bool result = ExecuteCode(cf, ca);
    if (!result) {
        fprintf(stderr, "Execution for method '%s' failed!\n", cf->ConstantPool[method->NameIndex - 1].As.Utf8);
    }
    FREE_CURRENT_FRAME();

    CodeAttributeDestroy(ca);
    return result;
}