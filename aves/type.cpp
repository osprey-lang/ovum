#include "aves_type.h"
#include <stddef.h>

AVES_API void CDECL aves_reflection_Type_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(TypeInst));

	Type_AddNativeField(type, offsetof(TypeInst,name), NativeFieldType::STRING);
}

int GetMemberSearchFlags(ThreadHandle thread, Value *arg, MemberSearchFlags *result)
{
	if (arg->type != Types::reflection.MemberSearchFlags)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::flags); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentError, 2);
	}

	*result = (MemberSearchFlags)arg->v.integer;
	RETURN_SUCCESS;
}

bool MatchMember(MemberHandle member, MemberSearchFlags flags, MemberKind kind)
{
	if (!member)
		return false;

	if (kind != MemberKind::INVALID && Member_GetKind(member) != kind)
		return false;

	switch (flags & MemberSearchFlags::ACCESSIBILITY)
	{
	case MemberSearchFlags::NONE:
		return false; // never matches
	case MemberSearchFlags::NON_PUBLIC:
		if (Member_GetAccessLevel(member) == MemberAccess::PUBLIC)
			return false;
		break;
	case MemberSearchFlags::PUBLIC:
		if (Member_GetAccessLevel(member) != MemberAccess::PUBLIC)
			return false;
		break;
	case MemberSearchFlags::ACCESSIBILITY:
		break; // always matches
	}

	switch (flags & MemberSearchFlags::INSTANCENESS)
	{
	case MemberSearchFlags::NONE:
		return false; // never matches
	case MemberSearchFlags::INSTANCE:
		if (Member_IsStatic(member))
			return false;
		break;
	case MemberSearchFlags::STATIC:
		if (!Member_IsStatic(member))
			return false;
		break;
	case MemberSearchFlags::INSTANCENESS:
		break; // always matches
	}

	// Successfulness!
	return true;
}

MemberHandle GetSingleMember(ThreadHandle thread, TypeHandle type,
                             String *name, MemberSearchFlags flags, MemberKind kind)
{
	MemberHandle member;
	do
	{
		member = Type_GetMember(type, name);
		if (MatchMember(member, flags, kind))
			break;
		member = nullptr;

		if ((flags & MemberSearchFlags::DECLARED_ONLY) == MemberSearchFlags::NONE)
			type = Type_GetBaseType(type);
		else
			type = nullptr;
	} while (type != nullptr);

	return member;
}

int HandleToMember(ThreadHandle thread, MemberHandle member)
{
	int r = OVUM_SUCCESS;
	if (member)
	{
		Value handle;
		handle.type = Types::reflection.NativeHandle;
		handle.v.instance = (uint8_t*)member;
		VM_Push(thread, &handle);

		TypeHandle type;
		switch (Member_GetKind(member))
		{
		case MemberKind::METHOD:
			if (Method_IsConstructor(member))
				type = Types::reflection.Constructor;
			else
				type = Types::reflection.Method;
			break;
		case MemberKind::FIELD:
			type = Types::reflection.Field;
			break;
		case MemberKind::PROPERTY:
			type = Types::reflection.Property;
			break;
		default:
			return VM_ThrowError(thread);
		}
		r = GC_Construct(thread, type, 1, nullptr);
	}
	else
		// Not found
		VM_PushNull(thread);
	return r;
}

int GetAllMembers(ThreadHandle thread, TypeHandle type,
                  MemberSearchFlags flags, MemberKind kind)
{
	int __status;
	{
		Value *list = VM_Local(thread, 0);
		VM_PushInt(thread, 5);
		CHECKED(GC_Construct(thread, GetType_List(thread), 1, list));

		// Make sure the list is always on the stack.
		VM_Push(thread, list);

		TypeMemberIterator iter(type, (flags & MemberSearchFlags::DECLARED_ONLY) == MemberSearchFlags::NONE);
		while (iter.MoveNext())
		{
			if (MatchMember(iter.Current(), flags, kind))
			{
				CHECKED(HandleToMember(thread, iter.Current()));
				// On stack:
				//        list
				//  (top) member
				Value ignore;
				CHECKED(VM_InvokeMember(thread, strings::add, 1, &ignore));
				// And push the list back
				VM_Push(thread, list);
			}
		}
		// The list is on the top of the stack; just return now!
	}
	__status = OVUM_SUCCESS;
__retStatus:
	return __status;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_handle)
{
	TypeInst *inst = THISV.Get<TypeInst>();

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.v.instance = (uint8_t*)inst->type;
	VM_Push(thread, &handle);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_f_name)
{
	TypeInst *inst = THISV.Get<TypeInst>();

	if (inst->name == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, inst->name);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_reflection_Type_set_f_name)
{
	TypeInst *inst = THISV.Get<TypeInst>();
	if (IS_NULL(args[1]))
		inst->name = nullptr;
	else
		inst->name = args[1].v.string;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_fullName)
{
	TypeInst *inst = THISV.Get<TypeInst>();
	VM_PushString(thread, Type_GetFullName(inst->type));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_declaringModule);

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_get_baseType)
{
	TypeInst *inst = THISV.Get<TypeInst>();

	TypeHandle baseType = Type_GetBaseType(inst->type);
	if (baseType == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	Value result;
	CHECKED(Type_GetTypeToken(thread, baseType, &result));
	VM_Push(thread, &result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isPrivate)
{
	TypeInst *inst = THISV.Get<TypeInst>();
	TypeFlags flags = Type_GetFlags(inst->type);
	VM_PushBool(thread, (flags & TypeFlags::PRIVATE) == TypeFlags::PRIVATE);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isAbstract)
{
	TypeInst *inst = THISV.Get<TypeInst>();
	TypeFlags flags = Type_GetFlags(inst->type);
	VM_PushBool(thread, (flags & TypeFlags::ABSTRACT) == TypeFlags::ABSTRACT);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isInheritable)
{
	TypeInst *inst = THISV.Get<TypeInst>();
	TypeFlags flags = Type_GetFlags(inst->type);
	VM_PushBool(thread, (flags & TypeFlags::SEALED) == TypeFlags::NONE);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isStatic)
{
	TypeInst *inst = THISV.Get<TypeInst>();
	TypeFlags flags = Type_GetFlags(inst->type);
	VM_PushBool(thread, (flags & TypeFlags::STATIC) == TypeFlags::STATIC);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isPrimitive)
{
	TypeInst *inst = THISV.Get<TypeInst>();
	TypeFlags flags = Type_GetFlags(inst->type);
	VM_PushBool(thread, (flags & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_canIterate)
{
	TypeInst *inst = THISV.Get<TypeInst>();

	bool result = false;
	// If there is a public method called ".iter", we can iterate over the type.
	// Pass null into fromType to exclude non-public members.
	MemberHandle iterMember = Type_FindMember(inst->type, strings::_iter, nullptr);
	MethodHandle method;
	if (iterMember && (method = Member_ToMethod(iterMember)))
		// The method also has to contain an overload that takes 0 arguments.
		result = Method_Accepts(method, 0);

	VM_PushBool(thread, result);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_createInstance)
{
	// createInstance(arguments is List|null, nonPublic is Boolean)

	TypeInst *inst = THISV.Get<TypeInst>();

	MemberHandle ctor = Type_GetMember(inst->type, strings::_new);
	if (args[2].v.integer == 0 && Member_GetAccessLevel(ctor) != MemberAccess::PUBLIC)
		// No public constructor, and nonPublic is false
		return VM_ThrowErrorOfType(thread, Types::InvalidStateError, 0);

	// Push arguments
	uint32_t argCount = 0;
	if (!IS_NULL(args[1]))
	{
		ListInst *arguments = args[1].v.list;
		argCount = (uint32_t)arguments->length;
		for (int32_t i = 0; i < arguments->length; i++)
			VM_Push(thread, arguments->values + i);
	}

	CHECKED(GC_Construct(thread, inst->type, (uint16_t)argCount, nullptr));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Type_inheritsFromInternal)
{
	// This is written in native code so we don't have
	// to construct type tokens for every base type

	TypeHandle self  = THISV.Get<TypeInst>()->type;
	TypeHandle other = args[1].Get<TypeInst>()->type;

	while (self && self != other)
		self = Type_GetBaseType(self);

	VM_PushBool(thread, self == other);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Type_isInstance)
{
	// isInstance(value)
	TypeHandle thisType  = THISV.Get<TypeInst>()->type;
	TypeHandle valueType = args[1].type;

	bool isType = false;
	while (valueType != nullptr)
	{
		if (valueType == thisType)
		{
			isType = true;
			break;
		}
		valueType = Type_GetBaseType(valueType);
	}

	VM_PushBool(thread, isType);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getField)
{
	// getField(name, flags)

	TypeInst *inst = THISV.Get<TypeInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	MemberHandle member = GetSingleMember(thread, inst->type, name, flags, MemberKind::FIELD);
	CHECKED(HandleToMember(thread, member));
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getFields)
{
	// getFields(flags)
	TypeInst *inst = THISV.Get<TypeInst>();

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->type, flags, MemberKind::FIELD));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getMethod)
{
	// getMethod(name, flags)

	TypeInst *inst = THISV.Get<TypeInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	MemberHandle member = GetSingleMember(thread, inst->type, name, flags, MemberKind::METHOD);
	CHECKED(HandleToMember(thread, member));
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getMethods)
{
	// getMethods(flags)
	TypeInst *inst = THISV.Get<TypeInst>();

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->type, flags, MemberKind::METHOD));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getProperty)
{
	// getProperty(name, flags)

	TypeInst *inst = THISV.Get<TypeInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	MemberHandle member = GetSingleMember(thread, inst->type, name, flags, MemberKind::PROPERTY);
	CHECKED(HandleToMember(thread, member));
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getProperties)
{
	// getProperties(flags)
	TypeInst *inst = THISV.Get<TypeInst>();

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->type, flags, MemberKind::PROPERTY));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getMember)
{
	// getMember(name, flags)

	TypeInst *inst = THISV.Get<TypeInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	MemberHandle member = GetSingleMember(thread, inst->type, name, flags, MemberKind::INVALID);
	CHECKED(HandleToMember(thread, member));
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Type_getMembers)
{
	// getMembers(flags)
	TypeInst *inst = THISV.Get<TypeInst>();

	MemberSearchFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->type, flags, MemberKind::INVALID));
}
END_NATIVE_FUNCTION

AVES_API int CDECL InitTypeToken(ThreadHandle thread, void *basePtr, TypeHandle type)
{
	TypeInst *inst = reinterpret_cast<TypeInst*>(basePtr);
	inst->type = type;
	RETURN_SUCCESS;
}