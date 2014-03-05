#pragma once

#ifndef AVES__METHOD_H
#define AVES__METHOD_H

#include "aves.h"

AVES_API void aves_Method_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Method_new);
AVES_API NATIVE_FUNCTION(aves_Method_get_hasInstance);
AVES_API NATIVE_FUNCTION(aves_Method_accepts);
AVES_API NATIVE_FUNCTION(aves_Method_opEquals);

bool aves_Method_getReferences(void *basePtr, unsigned int *valc, Value **target, int32_t *state);

#endif // AVES__METHOD_H