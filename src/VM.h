#ifndef CODE_H
#define CODE_H

#include "ClassFile.h"
#include <stdbool.h>

bool ExecuteMethod(const ClassFile* cf, const MethodInfo* method);

#endif //CODE_H
