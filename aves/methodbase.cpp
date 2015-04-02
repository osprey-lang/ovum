#include "aves_methodbase.h"
#include <stddef.h>

#define _M(val)     reinterpret_cast<MethodBaseInst*>((val).instance)

AVES_API void CDECL aves_reflection_MethodBase_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(MethodBaseInst));

	Type_AddNativeField(type, offsetof(MethodBaseInst,cachedName), NativeFieldType::STRING);
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_new)
{
	// new(handle)

	if (args[1].type != Types::reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentError, 2);
	}

	MethodBaseInst *inst = _M(THISV);
	inst->method = (MethodHandle)args[1].instance;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_accessLevel)
{
	MethodBaseInst *inst = _M(THISV);

	Value access;
	access.type = Types::reflection.AccessLevel;
	access.integer = (int)Member_GetAccessLevel(inst->method);
	VM_Push(thread, &access);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_handle)
{
	MethodBaseInst *inst = _M(THISV);

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)inst->method;

	VM_Push(thread, &handle);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_internalName)
{
	MethodBaseInst *inst = _M(THISV);
	VM_PushString(thread, Member_GetName(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_cachedName)
{
	MethodBaseInst *inst = _M(THISV);

	if (inst->cachedName == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, inst->cachedName);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_set_cachedName)
{
	MethodBaseInst *inst = _M(THISV);
	if (IS_NULL(args[1]))
		inst->cachedName = nullptr;
	else
		inst->cachedName = args[1].common.string;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_MethodBase_get_declaringType)
{
	MethodBaseInst *inst = _M(THISV);

	Value typeToken;
	CHECKED(Type_GetTypeToken(thread, Member_GetDeclType(inst->method), &typeToken));

	VM_Push(thread, &typeToken);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isGlobal)
{
	MethodBaseInst *inst = _M(THISV);
	VM_PushBool(thread, Member_GetDeclType(inst->method) == nullptr);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isStatic)
{
	MethodBaseInst *inst = _M(THISV);
	VM_PushBool(thread, Member_IsStatic(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isConstructor)
{
	MethodBaseInst *inst = _M(THISV);
	VM_PushBool(thread, Method_IsConstructor(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isImpl)
{
	MethodBaseInst *inst = _M(THISV);
	VM_PushBool(thread, Member_IsImpl(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_overloadCount)
{
	MethodBaseInst *inst = _M(THISV);
	VM_PushInt(thread, Method_GetOverloadCount(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_getOverloadHandle)
{
	// getOverloadHandle(index is Int)
	MethodBaseInst *inst = _M(THISV);
	int32_t index = (int32_t)args[1].integer;

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)Method_GetOverload(inst->method, index);
	VM_Push(thread, &handle);

	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_MethodBase_invoke)
{
	// invoke(instance, arguments is List|null)

	MethodBaseInst *inst = _M(THISV);

	// Push instance
	VM_Push(thread, args + 1);
	// Push arguments
	uint32_t argCount = 0;
	if (!IS_NULL(args[2]))
	{
		ListInst *arguments = args[2].common.list;
		argCount = (uint32_t)arguments->length;
		for (int32_t i = 0; i < arguments->length; i++)
			VM_Push(thread, arguments->values + i);
	}

	// Leave result on stack
	CHECKED(VM_InvokeMethod(thread, inst->method, argCount, nullptr));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Method_get_baseMethod)
{
	MethodBaseInst *inst = _M(THISV);

	MethodHandle baseMethod = Method_GetBaseMethod(inst->method);
	if (baseMethod != nullptr)
	{
		Value handle;
		handle.type = Types::reflection.NativeHandle;
		handle.instance = (uint8_t*)baseMethod;
		VM_Push(thread, &handle);

		CHECKED(GC_Construct(thread, Types::reflection.Method, 1, nullptr));
	}
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION