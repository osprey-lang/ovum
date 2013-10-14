#pragma once

#ifndef AVES__ERROR_H
#define AVES__ERROR_H

#include "aves.h"

AVES_API void aves_Error_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Error_new);

AVES_API NATIVE_FUNCTION(aves_Error_get_message);
AVES_API NATIVE_FUNCTION(aves_Error_get_stackTrace);
AVES_API NATIVE_FUNCTION(aves_Error_get_innerError);
AVES_API NATIVE_FUNCTION(aves_Error_get_data);

AVES_API NATIVE_FUNCTION(aves_Error_toString);

bool aves_Error_getReferences(void *basePtr, unsigned int &valc, Value **target);

#endif // AVES__ERROR_H