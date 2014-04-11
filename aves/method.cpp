#include <limits.h>
#include "aves_method.h"

#define _M(value)	((value).common.method)

AVES_API void aves_Method_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(MethodInst));
	Type_SetReferenceGetter(type, aves_Method_getReferences);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Method_new)
{
	if (IS_NULL(args[1]))
	{
		VM_PushString(thread, strings::value);
		CHECKED(GC_Construct(thread, Types::ArgumentNullError, 1, nullptr));
		return VM_Throw(thread);
	}

	MemberHandle invocator = Type_FindMember(args[1].type, strings::_call, args[0].type);
	if (invocator == nullptr ||
		Member_GetKind(invocator) != MemberKind::METHOD ||
		Member_IsStatic(invocator))
	{
		VM_PushString(thread, error_strings::ValueNotInvokable); // message
		VM_PushString(thread, strings::value); // paramName
		CHECKED(GC_Construct(thread, Types::ArgumentError, 2, nullptr));
		return VM_Throw(thread);
	}

	MethodInst *method = args[0].common.method;
	method->instance = args[1];
	method->method = (MethodHandle)invocator;
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Method_get_hasInstance)
{
	MethodInst *method = _M(THISV);

	VM_PushBool(thread, !IS_NULL(method->instance));
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_Method_accepts)
{
	MethodInst *method = _M(THISV);

	CHECKED(IntFromValue(thread, args + 1));
	int64_t argCount = args[1].integer;

	if (argCount < 0 || argCount > INT_MAX)
		VM_PushBool(thread, false);
	else
		VM_PushBool(thread, Method_Accepts(method->method, (int)argCount));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Method_opEquals)
{
	if (!IsType(args + 1, args[0].type))
	{
		VM_PushBool(thread, false);
		RETURN_SUCCESS;
	}

	MethodInst *a = _M(args[0]);
	MethodInst *b = _M(args[1]);

	VM_PushBool(thread, a->method == b->method &&
		IsSameReference(&a->instance, &b->instance));
	RETURN_SUCCESS;
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