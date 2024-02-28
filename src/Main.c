#include <stdio.h>
#include <stdlib.h>

#include "ClassFile.h"
#include "Utils.h"
#include "VM.h"

int Run(const char* filePath, const char* methodName)
{
    size_t size;
    uint8_t* fileData = ReadFileToBuffer(filePath, &size);

    if (!fileData) {
        return 1;
    }

    const ClassFile* classFile = ClassFileCreate(fileData, size);
    if (classFile == NULL) {
        fprintf(stderr, "Failed to create ClassFile.\n");
        free(fileData);
        return 1;
    }

    free(fileData);

#if defined(APP_DEBUG)
    printf("Magic: %x\n", classFile->Magic);
    printf("Version: %d.%d\n", classFile->Major, classFile->Minor);
#endif

    const MethodInfo* methodToRun = FindMethodByName(classFile, methodName);
    if (!methodToRun) {
        const Constant* class = &classFile->ConstantPool[classFile->ThisClass - 1];
        assert(class->Type == CONST_CLASS);
        const Constant* className = &classFile->ConstantPool[class->As.Class.NameIndex - 1];
        assert(className->Type == CONST_UTF8);
        fprintf(stderr, "Method '%s' does not exist in class '%s'\n", methodName, className->As.Utf8);
    } else {
        ExecuteMethod(classFile, methodToRun);
    }

    ClassFileDestroy(classFile);
    return 0;
}

int main(const int argc, const char** argv)
{
    if (argc < 3) {
        printf("Usage: %s <file_path> <method_name>\n", argv[0]);
        return 0;
    }

    return Run(argv[1], argv[2]);
}