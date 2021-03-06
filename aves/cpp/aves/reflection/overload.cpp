#include "overload.h"
#include "../../aves_state.h"
#include <stddef.h>

using namespace aves;

AVES_API int OVUM_CDECL aves_reflection_Overload_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(OverloadInst));

	int r = Type_AddNativeField(type, offsetof(OverloadInst,method), NativeFieldType::VALUE);
	if (r != OVUM_SUCCESS)
		return r;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Overload_new)
{
	// new(handle, method, index)
	Aves *aves = Aves::Get(thread);

	if (args[1].type != aves->aves.reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
	}

	CHECKED(IntFromValue(thread, args + 3));

	OverloadInst *inst = THISV.Get<OverloadInst>();
	inst->overload = (OverloadHandle)args[1].v.instance;
	inst->index = (size_t)args[3].v.integer;
	inst->method = args[2];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_handle)
{
	Aves *aves = Aves::Get(thread);

	OverloadInst *inst = THISV.Get<OverloadInst>();

	Value handle;
	handle.type = aves->aves.reflection.NativeHandle;
	handle.v.instance = (uint8_t*)inst->overload;

	VM_Push(thread, &handle);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_method)
{
	OverloadInst *inst = THISV.Get<OverloadInst>();
	VM_Push(thread, &inst->method);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_index)
{
	OverloadInst *inst = THISV.Get<OverloadInst>();
	VM_PushInt(thread, (int64_t)inst->index);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isOverridable)
{
	OverloadInst *inst = THISV.Get<OverloadInst>();
	uint32_t flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & OVUM_OVERLOAD_VIRTUAL) == OVUM_OVERLOAD_VIRTUAL);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isAbstract)
{
	OverloadInst *inst = THISV.Get<OverloadInst>();
	uint32_t flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & OVUM_OVERLOAD_ABSTRACT) == OVUM_OVERLOAD_ABSTRACT);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isVariadic)
{
	OverloadInst *inst = THISV.Get<OverloadInst>();
	uint32_t flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & OVUM_OVERLOAD_VARIADIC) == OVUM_OVERLOAD_VARIADIC);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isNative)
{
	OverloadInst *inst = THISV.Get<OverloadInst>();
	uint32_t flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & OVUM_OVERLOAD_NATIVE) == OVUM_OVERLOAD_NATIVE);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_paramCount)
{
	OverloadInst *inst = THISV.Get<OverloadInst>();
	VM_PushInt(thread, Overload_GetParamCount(inst->overload));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Overload_getCurrentOverload)
{
	Aves *aves = Aves::Get(thread);

	// Get the overload of the previous stack frame
	OverloadHandle overload = VM_GetExecutingOverload(thread, 1);
	if (overload == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	// Overload's constructor takes (handle, method, index);
	// let's push all three!

	// Push a NativeHandle for the overload
	Value handle;
	handle.type = aves->aves.reflection.NativeHandle;
	handle.v.instance = (uint8_t*)overload;
	VM_Push(thread, &handle);

	MethodHandle method = Overload_GetMethod(overload);
	// Push a NativeHandle for the method constructor
	handle.v.instance = (uint8_t*)method;
	VM_Push(thread, &handle);

	// Select type Method or Constructor based on the CTOR flag
	TypeHandle type = aves->aves.reflection.Method;
	if (Method_IsConstructor(method))
		type = aves->aves.reflection.Constructor;

	// Leave Method/Constructor on stack
	CHECKED(GC_Construct(thread, type, 1, nullptr));

	// And now find the index of the overload
	size_t index = 0;
	for (size_t count = Method_GetOverloadCount(method); index < count; index++)
		if (Method_GetOverload(method, index) == overload)
			break;
	OVUM_ASSERT(index < Method_GetOverloadCount(method));
	VM_PushInt(thread, (int64_t)index);

	// Stack now contains:
	//        handle: NativeHandle (overload)
	//        method: Method/Constructor
	//  (top) index:  Int
	// Let's call new Overload(handle, method, index), and
	// return the result!
	CHECKED(GC_Construct(thread, aves->aves.reflection.Overload, 3, nullptr));
}
END_NATIVE_FUNCTION


AVES_API int OVUM_CDECL aves_reflection_Parameter_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ParamInst));

	int r = Type_AddNativeField(type, offsetof(ParamInst,param.name), NativeFieldType::STRING);
	if (r != OVUM_SUCCESS)
		return r;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Parameter_new)
{
	// new(overload, index)
	Aves *aves = Aves::Get(thread);

	if (args[1].type != aves->aves.reflection.Overload)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::overload); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
	}
	OverloadInst *ovl = args[1].Get<OverloadInst>();

	ParamInst *inst = THISV.Get<ParamInst>();

	CHECKED(IntFromValue(thread, args + 2));
	bool found = false;
	if (args[2].v.uinteger <= OVLOCALS_MAX)
		found = Overload_GetParameter(ovl->overload, (ovlocals_t)args[2].v.integer, &inst->param);
	if (!found)
	{
		VM_PushString(thread, strings::index);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	inst->index = (ovlocals_t)args[2].v.integer;
	inst->overload = args[1];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_overload)
{
	ParamInst *inst = THISV.Get<ParamInst>();
	VM_Push(thread, &inst->overload);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_index)
{
	ParamInst *inst = THISV.Get<ParamInst>();
	VM_PushInt(thread, inst->index);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_name)
{
	ParamInst *inst = THISV.Get<ParamInst>();
	VM_PushString(thread, inst->param.name);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isByRef)
{
	ParamInst *inst = THISV.Get<ParamInst>();
	VM_PushBool(thread, inst->param.isByRef);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isOptional)
{
	ParamInst *inst = THISV.Get<ParamInst>();
	VM_PushBool(thread, inst->param.isOptional);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isVariadic)
{
	ParamInst *inst = THISV.Get<ParamInst>();
	VM_PushBool(thread, inst->param.isVariadic);
	RETURN_SUCCESS;
}
