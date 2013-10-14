#include "aves_type.h"

#define _T(val)		reinterpret_cast<TypeInst*>((val).instance)

AVES_API void aves_Type_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(TypeInst));
	Type_SetReferenceGetter(type, aves_Type_getReferences);
}

AVES_API void InitTypeToken(ThreadHandle thread, void *basePtr, TypeHandle type)
{
	TypeInst *inst = reinterpret_cast<TypeInst*>(basePtr);
	inst->type = type;
}

bool aves_Type_getReferences(void *basePtr, unsigned int &valc, Value **target)
{
	valc = 0;
	return false;
}