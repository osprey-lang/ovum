#include "aves_method.h"

#define _M(value)	((value).common.method)

AVES_API void aves_Method_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(MethodInst));
	Type_SetReferenceGetter(type, aves_Method_getReferences);
}

AVES_API NATIVE_FUNCTION(aves_Method_new)
{
	// TODO: aves_Method_new
	VM_ThrowError(thread);
}
AVES_API NATIVE_FUNCTION(aves_Method_get_hasInstance)
{
	MethodInst *method = _M(THISV);

	VM_PushBool(thread, !IS_NULL(method->instance));
}
AVES_API NATIVE_FUNCTION(aves_Method_accepts)
{
	MethodInst *method = _M(THISV);

	IntFromValue(thread, args + 1);
	int64_t argCount = args[1].integer;

	if (argCount < 0 || argCount > INT32_MAX)
		VM_PushBool(thread, false);
	else
		VM_PushBool(thread, Method_Accepts(method->method, (int32_t)argCount));
}

AVES_API NATIVE_FUNCTION(aves_Method_opEquals)
{
	if (!IsType(args + 1, args[0].type))
	{
		VM_PushBool(thread, false);
		return;
	}

	MethodInst *a = _M(args[0]);
	MethodInst *b = _M(args[1]);

	VM_PushBool(thread, a->method == b->method &&
		IsSameReference(&a->instance, &b->instance));
}

bool aves_Method_getReferences(void *basePtr, unsigned int *valc, Value **target, int32_t *state)
{
	MethodInst *method = reinterpret_cast<MethodInst*>(basePtr);
	if (!IS_NULL(method->instance))
	{
		*valc = 1;
		*target = &method->instance;
	}
	else
	{
		valc = 0;
	}

	return false;
}