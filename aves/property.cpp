#include "aves_property.h"
#include <stddef.h>

#define _P(val)     reinterpret_cast<PropertyInst*>((val).instance)

AVES_API void CDECL aves_reflection_Property_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(PropertyInst));

	Type_AddNativeField(type, offsetof(PropertyInst,fullName), NativeFieldType::STRING);
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_new)
{
	// new(handle)

	if (args[1].type != Types::reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentError, 2);
	}

	PropertyInst *inst = THISV.Get<PropertyInst>();
	inst->property = (PropertyHandle)args[1].v.instance;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_accessLevel)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();

	Value access;
	access.type = Types::reflection.AccessLevel;
	access.v.integer = (int)Member_GetAccessLevel(inst->property);
	VM_Push(thread, &access);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_handle)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();

	Value handleValue;
	handleValue.type = Types::reflection.NativeHandle;
	handleValue.v.instance = (uint8_t*)inst->property;

	VM_Push(thread, &handleValue);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_name)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();
	VM_PushString(thread, Member_GetName(inst->property));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_f_fullName)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();

	if (inst->fullName == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, inst->fullName);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_reflection_Property_set_f_fullName)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();
	if (IS_NULL(args[1]))
		inst->fullName = nullptr;
	else
		inst->fullName = args[1].v.string;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Property_get_declaringType)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();

	Value typeToken;
	CHECKED(Type_GetTypeToken(thread, Member_GetDeclType(inst->property), &typeToken));

	VM_Push(thread, &typeToken);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_isStatic)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();
	VM_PushBool(thread, Member_IsStatic(inst->property));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_canRead)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();
	VM_PushBool(thread, Property_GetGetter(inst->property) != nullptr);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_canWrite)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();
	VM_PushBool(thread, Property_GetSetter(inst->property) != nullptr);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Property_get_getterMethod)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();

	MethodHandle getter = Property_GetGetter(inst->property);
	if (getter == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.v.instance = (uint8_t*)getter;
	VM_Push(thread, &handle);

	CHECKED(GC_Construct(thread, Types::reflection.Method, 1, nullptr));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Property_get_setterMethod)
{
	PropertyInst *inst = THISV.Get<PropertyInst>();

	MethodHandle setter = Property_GetSetter(inst->property);
	if (setter == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.v.instance = (uint8_t*)setter;
	VM_Push(thread, &handle);

	CHECKED(GC_Construct(thread, Types::reflection.Method, 1, nullptr));
}
END_NATIVE_FUNCTION