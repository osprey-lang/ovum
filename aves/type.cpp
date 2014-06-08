#include "aves_type.h"

#define _T(val)		reinterpret_cast<TypeInst*>((val).instance)

AVES_API void aves_reflection_Type_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(TypeInst));
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_fullName)
{
	TypeInst *inst = _T(THISV);
	VM_PushString(thread, Type_GetFullName(inst->type));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_get_baseType)
{
	TypeInst *inst = _T(THISV);

	TypeHandle baseType = Type_GetBaseType(inst->type);
	if (baseType == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	Value result;
	CHECKED(Type_GetTypeToken(thread, baseType, &result));
	VM_Push(thread, &result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Type_inheritsFromInternal)
{
	// This is written in native code so we don't have
	// to construct type tokens for every base type

	TypeHandle self  = _T(THISV)->type;
	TypeHandle other = _T(args[1])->type;

	while (self && self != other)
		self = Type_GetBaseType(self);

	VM_PushBool(thread, self == other);
	RETURN_SUCCESS;
}

AVES_API int InitTypeToken(ThreadHandle thread, void *basePtr, TypeHandle type)
{
	TypeInst *inst = reinterpret_cast<TypeInst*>(basePtr);
	inst->type = type;
	RETURN_SUCCESS;
}