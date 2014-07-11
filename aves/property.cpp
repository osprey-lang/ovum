#include "aves_property.h"
#include <stddef.h>

#define _P(val)     reinterpret_cast<PropertyInst*>((val).instance)

AVES_API void CDECL aves_reflection_Property_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(PropertyInst));

	Type_AddNativeField(type, offsetof(PropertyInst,fullName), NativeFieldType::STRING);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Property_new)
{
	// new(handle)

	if (args[1].type != Types::reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		CHECKED(GC_Construct(thread, Types::ArgumentError, 2, nullptr));
		return VM_Throw(thread);
	}

	PropertyInst *inst = _P(THISV);
	inst->property = (PropertyHandle)args[1].instance;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_accessLevel)
{
	PropertyInst *inst = _P(THISV);

	Value access;
	access.type = Types::reflection.AccessLevel;
	access.integer = (int)Member_GetAccessLevel(inst->property);
	VM_Push(thread, &access);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_handle)
{
	PropertyInst *inst = _P(THISV);

	Value handleValue;
	handleValue.type = Types::reflection.NativeHandle;
	handleValue.instance = (uint8_t*)inst->property;

	VM_Push(thread, &handleValue);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_name)
{
	PropertyInst *inst = _P(THISV);
	VM_PushString(thread, Member_GetName(inst->property));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_f_fullName)
{
	PropertyInst *inst = _P(THISV);

	if (inst->fullName == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, inst->fullName);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_reflection_Property_set_f_fullName)
{
	PropertyInst *inst = _P(THISV);
	if (IS_NULL(args[1]))
		inst->fullName = nullptr;
	else
		inst->fullName = args[1].common.string;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Property_get_declaringType)
{
	PropertyInst *inst = _P(THISV);

	Value typeToken;
	CHECKED(Type_GetTypeToken(thread, Member_GetDeclType(inst->property), &typeToken));

	VM_Push(thread, &typeToken);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_isStatic)
{
	PropertyInst *inst = _P(THISV);
	VM_PushBool(thread, Member_IsStatic(inst->property));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_canRead)
{
	PropertyInst *inst = _P(THISV);
	VM_PushBool(thread, Property_GetGetter(inst->property) != nullptr);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_canWrite)
{
	PropertyInst *inst = _P(THISV);
	VM_PushBool(thread, Property_GetSetter(inst->property) != nullptr);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Property_get_getterMethod)
{
	PropertyInst *inst = _P(THISV);

	MethodHandle getter = Property_GetGetter(inst->property);
	if (getter == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)getter;
	VM_Push(thread, &handle);

	CHECKED(GC_Construct(thread, Types::reflection.Method, 1, nullptr));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Property_get_setterMethod)
{
	PropertyInst *inst = _P(THISV);

	MethodHandle setter = Property_GetSetter(inst->property);
	if (setter == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)setter;
	VM_Push(thread, &handle);

	CHECKED(GC_Construct(thread, Types::reflection.Method, 1, nullptr));
}
END_NATIVE_FUNCTION