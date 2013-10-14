#pragma once

#ifndef AVES__TYPE_H
#define AVES__TYPE_H

#include "aves.h"

typedef struct TypeInst_S
{
	TypeHandle type;
} TypeInst;

AVES_API void aves_Type_init(TypeHandle type);

AVES_API void InitTypeToken(ThreadHandle thread, void *basePtr, TypeHandle type);

bool aves_Type_getReferences(void *basePtr, unsigned int &valc, Value **target);

#endif // AVES__TYPE_H