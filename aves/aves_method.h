#pragma once

#ifndef AVES__METHOD_H
#define AVES__METHOD_H

#include "aves.h"

AVES_API void aves_Method_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Method_new);
AVES_API NATIVE_FUNCTION(aves_Method_get_hasInstance);
AVES_API NATIVE_FUNCTION(aves_Method_accepts);

bool aves_Method_getReferences(void *basePtr, unsigned int &valc, Value **target);

#endif // AVES__METHOD_H