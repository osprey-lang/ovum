#include <limits.h>
#include "aves_method.h"
#include "aves_state.h"
#include <cstddef>

using namespace aves;

AVES_API void CDECL aves_Method_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(MethodInst));

	Type_AddNativeField(type, offsetof(MethodInst, instance), NativeFieldType::VALUE);
}

AVES_API NATIVE_FUNCTION(aves_Method_new)
{
	Aves *aves = Aves::Get(thread);

	if (IS_NULL(args[1]))
	{
		VM_PushString(thread, strings::value);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentNullError, 1);
	}

	MemberHandle invocator = Type_FindMember(args[1].type, strings::_call, THISV.type);
	if (invocator == nullptr ||
		Member_GetKind(invocator) != MemberKind::METHOD ||
		Member_IsStatic(invocator))
	{
		VM_PushString(thread, error_strings::ValueNotInvokable); // message
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 2);
	}

	MethodInst *method = THISV.v.method;
	method->instance = args[1];
	method->method = (MethodHandle)invocator;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Method_get_hasInstance)
{
	MethodInst *method = THISV.v.method;

	VM_PushBool(thread, !IS_NULL(method->instance));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Method_accepts)
{
	MethodInst *method = THISV.v.method;

	CHECKED(IntFromValue(thread, args + 1));
	int64_t argCount = args[1].v.integer;

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

	MethodInst *a = args[0].v.method;
	MethodInst *b = args[1].v.method;

	VM_PushBool(thread, a->method == b->method &&
		IsSameReference(&a->instance, &b->instance));
	RETURN_SUCCESS;
}