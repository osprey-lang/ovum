#pragma once

#ifndef AVES__TYPE_H
#define AVES__TYPE_H

#include "aves.h"

typedef struct TypeInst_S
{
	TypeHandle type;
} TypeInst;

AVES_API void aves_reflection_Type_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_fullName);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_baseType);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_inheritsFromInternal);

AVES_API int InitTypeToken(ThreadHandle thread, void *basePtr, TypeHandle type);

#endif // AVES__TYPE_H