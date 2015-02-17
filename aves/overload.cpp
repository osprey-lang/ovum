#include "aves_overload.h"
#include <stddef.h>
#include <cassert>

#define _O(val)     reinterpret_cast<OverloadInst*>((val).instance)

AVES_API void CDECL aves_reflection_Overload_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(OverloadInst));

	Type_AddNativeField(type, offsetof(OverloadInst,method), NativeFieldType::VALUE);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Overload_new)
{
	// new(handle, method, index)

	if (args[1].type != Types::reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentError, 2);
	}

	CHECKED(IntFromValue(thread, args + 3));

	OverloadInst *inst = _O(THISV);
	inst->overload = (OverloadHandle)args[1].instance;
	inst->index = (int32_t)args[3].integer;
	inst->method = args[2];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_handle)
{
	OverloadInst *inst = _O(THISV);

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)inst->overload;

	VM_Push(thread, &handle);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_method)
{
	OverloadInst *inst = _O(THISV);
	VM_Push(thread, &inst->method);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_index)
{
	OverloadInst *inst = _O(THISV);
	VM_PushInt(thread, inst->index);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isConstructor)
{
	OverloadInst *inst = _O(THISV);
	MethodFlags flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & MethodFlags::CTOR) == MethodFlags::CTOR);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isOverridable)
{
	OverloadInst *inst = _O(THISV);
	MethodFlags flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & MethodFlags::VIRTUAL) == MethodFlags::VIRTUAL);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isAbstract)
{
	OverloadInst *inst = _O(THISV);
	MethodFlags flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & MethodFlags::ABSTRACT) == MethodFlags::ABSTRACT);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isVariadic)
{
	OverloadInst *inst = _O(THISV);
	MethodFlags flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & MethodFlags::VARIADIC) != MethodFlags::NONE);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isNative)
{
	OverloadInst *inst = _O(THISV);
	MethodFlags flags = Overload_GetFlags(inst->overload);
	VM_PushBool(thread, (flags & MethodFlags::NATIVE) == MethodFlags::NATIVE);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_paramCount)
{
	OverloadInst *inst = _O(THISV);
	VM_PushInt(thread, Overload_GetParamCount(inst->overload));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Overload_getCurrentOverload)
{
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
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)overload;
	VM_Push(thread, &handle);

	MethodHandle method = Overload_GetMethod(overload);
	// Push a NativeHandle for the method constructor
	handle.instance = (uint8_t*)method;
	VM_Push(thread, &handle);

	// Select type Method or Constructor based on the CTOR flag
	TypeHandle type = Types::reflection.Method;
	if ((Overload_GetFlags(overload) & MethodFlags::CTOR) == MethodFlags::CTOR)
		type = Types::reflection.Constructor;

	// Leave Method/Constructor on stack
	CHECKED(GC_Construct(thread, type, 1, nullptr));

	// And now find the index of the overload
	int32_t index = 0;
	for (int32_t count = Method_GetOverloadCount(method); index < count; index++)
		if (Method_GetOverload(method, index) == overload)
			break;
	assert(index < Method_GetOverloadCount(method));
	VM_PushInt(thread, index);

	// Stack now contains:
	//        handle: NativeHandle (overload)
	//        method: Method/Constructor
	//  (top) index:  Int
	// Let's call new Overload(handle, method, index), and
	// return the result!
	CHECKED(GC_Construct(thread, Types::reflection.Overload, 3, nullptr));
}
END_NATIVE_FUNCTION

#define _P(val)     reinterpret_cast<ParamInst*>((val).instance)

AVES_API void CDECL aves_reflection_Parameter_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ParamInst));

	Type_AddNativeField(type, offsetof(ParamInst,param) + offsetof(ParamInfo,name), NativeFieldType::STRING);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Parameter_new)
{
	// new(overload, index)
	if (args[1].type != Types::reflection.Overload)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::overload); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentError, 2);
	}
	OverloadInst *ovl = _O(args[1]);

	ParamInst *inst = _P(THISV);

	CHECKED(IntFromValue(thread, args + 2));
	bool found = false;
	if (args[2].uinteger <= INT32_MAX)
		found = Overload_GetParameter(ovl->overload, (int32_t)args[2].integer, &inst->param);
	if (!found)
	{
		VM_PushString(thread, strings::index);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	inst->index = (int32_t)args[2].integer;
	inst->overload = args[1];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_overload)
{
	ParamInst *inst = _P(THISV);
	VM_Push(thread, &inst->overload);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_index)
{
	ParamInst *inst = _P(THISV);
	VM_PushInt(thread, inst->index);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_name)
{
	ParamInst *inst = _P(THISV);
	VM_PushString(thread, inst->param.name);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isByRef)
{
	ParamInst *inst = _P(THISV);
	VM_PushBool(thread, inst->param.isByRef);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isOptional)
{
	ParamInst *inst = _P(THISV);
	VM_PushBool(thread, inst->param.isOptional);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isVariadic)
{
	ParamInst *inst = _P(THISV);
	VM_PushBool(thread, inst->param.isVariadic);
	RETURN_SUCCESS;
}