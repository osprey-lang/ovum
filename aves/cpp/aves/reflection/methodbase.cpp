#include "methodbase.h"
#include "../../aves_state.h"
#include <stddef.h>

using namespace aves;

AVES_API void CDECL aves_reflection_MethodBase_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(MethodBaseInst));

	Type_AddNativeField(type, offsetof(MethodBaseInst,cachedName), NativeFieldType::STRING);
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_new)
{
	// new(handle)
	Aves *aves = Aves::Get(thread);

	if (args[1].type != aves->aves.reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
	}

	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	inst->method = (MethodHandle)args[1].v.instance;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_accessLevel)
{
	Aves *aves = Aves::Get(thread);

	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();

	Value access;
	access.type = aves->aves.reflection.AccessLevel;
	access.v.integer = (int)Member_GetAccessLevel(inst->method);
	VM_Push(thread, &access);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_handle)
{
	Aves *aves = Aves::Get(thread);

	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();

	Value handle;
	handle.type = aves->aves.reflection.NativeHandle;
	handle.v.instance = (uint8_t*)inst->method;

	VM_Push(thread, &handle);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_internalName)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	VM_PushString(thread, Member_GetName(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_cachedName)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();

	if (inst->cachedName == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, inst->cachedName);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_set_cachedName)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	if (IS_NULL(args[1]))
		inst->cachedName = nullptr;
	else
		inst->cachedName = args[1].v.string;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_MethodBase_get_declaringType)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();

	Value typeToken;
	CHECKED(Type_GetTypeToken(thread, Member_GetDeclType(inst->method), &typeToken));

	VM_Push(thread, &typeToken);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isGlobal)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	VM_PushBool(thread, Member_GetDeclType(inst->method) == nullptr);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isStatic)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	VM_PushBool(thread, Member_IsStatic(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isConstructor)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	VM_PushBool(thread, Method_IsConstructor(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isImpl)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	VM_PushBool(thread, Member_IsImpl(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_overloadCount)
{
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	VM_PushInt(thread, Method_GetOverloadCount(inst->method));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_getOverloadHandle)
{
	Aves *aves = Aves::Get(thread);

	// getOverloadHandle(index is Int)
	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();
	int32_t index = (int32_t)args[1].v.integer;

	Value handle;
	handle.type = aves->aves.reflection.NativeHandle;
	handle.v.instance = (uint8_t*)Method_GetOverload(inst->method, index);
	VM_Push(thread, &handle);

	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_MethodBase_invoke)
{
	// invoke(instance, arguments is List|null)

	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();

	// Push instance
	VM_Push(thread, args + 1);
	// Push arguments
	uint32_t argCount = 0;
	if (!IS_NULL(args[2]))
	{
		ListInst *arguments = args[2].v.list;
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
	Aves *aves = Aves::Get(thread);

	MethodBaseInst *inst = THISV.Get<MethodBaseInst>();

	MethodHandle baseMethod = Method_GetBaseMethod(inst->method);
	if (baseMethod != nullptr)
	{
		Value handle;
		handle.type = aves->aves.reflection.NativeHandle;
		handle.v.instance = (uint8_t*)baseMethod;
		VM_Push(thread, &handle);

		CHECKED(GC_Construct(thread, aves->aves.reflection.Method, 1, nullptr));
	}
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION