#include "aves_field.h"
#include <stddef.h>

#define _F(val)		reinterpret_cast<FieldInst*>((val).instance)

AVES_API void CDECL aves_reflection_Field_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(FieldInst));

	Type_AddNativeField(type, offsetof(FieldInst,fullName), NativeFieldType::STRING);
}

AVES_API NATIVE_FUNCTION(aves_reflection_Field_new)
{
	// new(handle)

	if (args[1].type != Types::reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentError, 2);
	}

	FieldInst *inst = _F(THISV);
	inst->field = (FieldHandle)args[1].instance;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_accessLevel)
{
	FieldInst *inst = _F(THISV);

	Value access;
	access.type = Types::reflection.AccessLevel;
	access.integer = (int)Member_GetAccessLevel(inst->field);
	VM_Push(thread, &access);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_handle)
{
	FieldInst *inst = _F(THISV);

	Value handleValue;
	handleValue.type = Types::reflection.NativeHandle;
	handleValue.instance = (uint8_t*)inst->field;

	VM_Push(thread, &handleValue);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_name)
{
	FieldInst *inst = _F(THISV);
	VM_PushString(thread, Member_GetName(inst->field));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_f_fullName)
{
	FieldInst *inst = _F(THISV);

	if (inst->fullName == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, inst->fullName);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_reflection_Field_set_f_fullName)
{
	FieldInst *inst = _F(THISV);
	if (IS_NULL(args[1]))
		inst->fullName = nullptr;
	else
		inst->fullName = args[1].common.string;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Field_get_declaringType)
{
	FieldInst *inst = _F(THISV);

	Value typeToken;
	CHECKED(Type_GetTypeToken(thread, Member_GetDeclType(inst->field), &typeToken));

	VM_Push(thread, &typeToken);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_isStatic)
{
	FieldInst *inst = _F(THISV);
	VM_PushBool(thread, Member_IsStatic(inst->field));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Field_getValue)
{
	// getValueInternal(instance)

	FieldInst *inst = _F(THISV);
	if (Member_IsStatic(inst->field))
	{
		CHECKED(VM_LoadStaticField(thread, inst->field, nullptr));
	}
	else
	{
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadField(thread, inst->field, nullptr));
	}
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Field_setValue)
{
	// setValueInternal(instance, value)

	FieldInst *inst = _F(THISV);
	if (Member_IsStatic(inst->field))
	{
		VM_Push(thread, args + 2); // value
		CHECKED(VM_StoreStaticField(thread, inst->field));
	}
	else
	{
		VM_Push(thread, args + 1); // instance
		VM_Push(thread, args + 2); // value
		CHECKED(VM_StoreField(thread, inst->field));
	}
}
END_NATIVE_FUNCTION