#include "aves_module.h"
#include "aves_type.h"
#include <stddef.h>

#define _M(val)     reinterpret_cast<ModuleInst*>((val).instance)

AVES_API void CDECL aves_reflection_Module_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ModuleInst));

	Type_AddNativeField(type, offsetof(ModuleInst,fileName), NativeFieldType::STRING);
	Type_AddNativeField(type, offsetof(ModuleInst,version),  NativeFieldType::VALUE);
}

int GetMemberSearchFlags(ThreadHandle thread, Value *arg, ModuleMemberFlags *result)
{
	if (arg->type != Types::reflection.MemberSearchFlags)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::flags); // paramName
		int r = GC_Construct(thread, Types::ArgumentError, 2, nullptr);
		if (r == OVUM_SUCCESS)
			r = VM_Throw(thread);
		return r;
	}

	MemberSearchFlags flags = (MemberSearchFlags)arg->integer;

	switch (flags & MemberSearchFlags::ACCESSIBILITY)
	{
	case MemberSearchFlags::NONE:
		// Never matches
		*result = ModuleMemberFlags::NONE;
		RETURN_SUCCESS;
	case MemberSearchFlags::PUBLIC:
		*result = ModuleMemberFlags::PUBLIC;
		break;
	case MemberSearchFlags::NON_PUBLIC:
		*result = ModuleMemberFlags::INTERNAL;
		break;
	case MemberSearchFlags::ACCESSIBILITY:
		*result = ModuleMemberFlags::PUBLIC | ModuleMemberFlags::INTERNAL;
		break;
	}

	if ((flags & MemberSearchFlags::STATIC) == MemberSearchFlags::NONE)
	{
		*result = ModuleMemberFlags::NONE;
		RETURN_SUCCESS;
	}
	// Ignore other flags: if we haven't returned yet, then
	// *result is non-zero, which means we can match something.
	// Since all global members are static, the instanceness
	// flag doesn't really matter, but we still require STATIC
	// to be present.

	RETURN_SUCCESS;
}

bool GetSingleMember(ModuleHandle module, String *name, ModuleMemberFlags access, ModuleMemberFlags kind, GlobalMember *result)
{
	if (Module_GetGlobalMember(module, name, true, result))
		return (result->flags & access) != ModuleMemberFlags::NONE &&
			(result->flags & kind) != ModuleMemberFlags::NONE;
	return false;
}

int ResultToMember(ThreadHandle thread, Value *module, GlobalMember *member)
{
	int r = OVUM_SUCCESS;

	switch (member->flags & ModuleMemberFlags::KIND)
	{
	case ModuleMemberFlags::TYPE:
		{
			Value typeToken;
			r = Type_GetTypeToken(thread, member->type, &typeToken);
			if (r == OVUM_SUCCESS)
				VM_Push(thread, &typeToken);
		}
		break;
	case ModuleMemberFlags::FUNCTION:
		{
			Value handle;
			handle.type = Types::reflection.NativeHandle;
			handle.instance = (uint8_t*)member->function;
			VM_Push(thread, &handle);
			r = GC_Construct(thread, Types::reflection.Method, 1, nullptr);
		}
		break;
	case ModuleMemberFlags::CONSTANT:
		{
			VM_Push(thread, module);
			VM_PushBool(thread, (member->flags & ModuleMemberFlags::INTERNAL) == ModuleMemberFlags::INTERNAL);
			VM_PushString(thread, member->name);
			VM_Push(thread, &member->constant);
			r = GC_Construct(thread, Types::reflection.GlobalConstant, 4, nullptr);
		}
		break;
	default:
		return VM_ThrowError(thread);
	}

	return r;
}

int GetAllMembers(ThreadHandle thread, ModuleHandle module, Value *moduleValue,
                  ModuleMemberFlags access, ModuleMemberFlags kind)
{
	int __status;
	{
		Value *list = VM_Local(thread, 0);
		VM_PushInt(thread, 5);
		CHECKED(GC_Construct(thread, GetType_List(), 1, list));

		// Make sure the list is always on the stack
		VM_Push(thread, list);

		ModuleMemberIterator iter(module);
		while (iter.MoveNext())
		{
			GlobalMember &member = iter.Current();
			if ((member.flags & access) != ModuleMemberFlags::NONE &&
				(member.flags & kind) != ModuleMemberFlags::NONE)
			{
				CHECKED(ResultToMember(thread, moduleValue, &member));
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

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_new)
{
	// new(handle)

	if (args[1].type != Types::reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		CHECKED(GC_Construct(thread, Types::ArgumentError, 2, nullptr));
		return VM_Throw(thread);
	}

	ModuleInst *inst = _M(THISV);
	inst->module = (ModuleHandle)args[1].instance;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_handle)
{
	ModuleInst *inst = _M(THISV);

	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)inst->module;
	VM_Push(thread, &handle);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_name)
{
	ModuleInst *inst = _M(THISV);
	VM_PushString(thread, Module_GetName(inst->module));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_get_version)
{
	ModuleInst *inst = _M(THISV);

	if (IS_NULL(inst->version))
	{
		ModuleVersion version;
		Module_GetVersion(inst->module, &version);
		VM_PushInt(thread, version.major);
		VM_PushInt(thread, version.minor);
		VM_PushInt(thread, version.build);
		VM_PushInt(thread, version.revision);
		CHECKED(GC_Construct(thread, Types::Version, 4, &inst->version));
	}

	VM_Push(thread, &inst->version);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_get_fileName)
{
	ModuleInst *inst = _M(THISV);

	if (inst->fileName == nullptr)
		CHECKED_MEM(inst->fileName = Module_GetFileName(thread, inst->module));

	if (inst->fileName == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, inst->fileName);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getType)
{
	// getType(name, flags)

	ModuleInst *inst = _M(THISV);

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].common.string;

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	GlobalMember member;
	if (GetSingleMember(inst->module, name, flags, ModuleMemberFlags::TYPE, &member))
		CHECKED(ResultToMember(thread, THISP, &member));
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getTypes)
{
	// getTypes(flags)

	ModuleInst *inst = _M(THISV);

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::TYPE));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getFunction)
{
	// getFunction(name, flags)

	ModuleInst *inst = _M(THISV);

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].common.string;

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	GlobalMember member;
	if (GetSingleMember(inst->module, name, flags, ModuleMemberFlags::FUNCTION, &member))
		CHECKED(ResultToMember(thread, THISP, &member));
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getFunctions)
{
	// getFunctions(flags)

	ModuleInst *inst = _M(THISV);

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::FUNCTION));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getGlobalConstant)
{
	// getGlobalConstant(name, flags)

	ModuleInst *inst = _M(THISV);

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].common.string;

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	GlobalMember member;
	if (GetSingleMember(inst->module, name, flags, ModuleMemberFlags::CONSTANT, &member))
		CHECKED(ResultToMember(thread, THISP, &member));
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getGlobalConstants)
{
	// getGlobalConstants(flags)

	ModuleInst *inst = _M(THISV);

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::CONSTANT));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getMember)
{
	// getMember(name, flags)

	ModuleInst *inst = _M(THISV);

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].common.string;

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 2, &flags));

	GlobalMember member;
	if (GetSingleMember(inst->module, name, flags, ModuleMemberFlags::KIND, &member))
		CHECKED(ResultToMember(thread, THISP, &member));
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getMembers)
{
	// getMembers(flags)

	ModuleInst *inst = _M(THISV);

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::KIND));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getCurrentModule)
{
	// Get the overload of the previous stack frame
	OverloadHandle overload = VM_GetExecutingOverload(thread, 1);
	if (overload == nullptr)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	MethodHandle method = Overload_GetMethod(overload);
	ModuleHandle module = Member_GetDeclModule(method);

	// Module's constructor takes a handle
	Value handle;
	handle.type = Types::reflection.NativeHandle;
	handle.instance = (uint8_t*)module;
	VM_Push(thread, &handle);

	CHECKED(GC_Construct(thread, Types::reflection.Module, 1, nullptr));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_find)
{
	// find(name is String, version is Version|null)

	PinnedAlias<String> name(args + 0);
	ModuleHandle result;
	if (IS_NULL(args[1]))
		result = FindModule(*name, nullptr);
	else
	{
		ModuleVersion version;
		Value field;
		// major
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadMember(thread, strings::major, &field));
		version.major = (int32_t)field.integer;
		// minor
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadMember(thread, strings::minor, &field));
		version.minor = (int32_t)field.integer;
		// build
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadMember(thread, strings::build, &field));
		version.build = (int32_t)field.integer;
		// revision
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadMember(thread, strings::revision, &field));
		version.revision = (int32_t)field.integer;

		result = FindModule(*name, &version);
	}

	if (result)
	{
		Value handle;
		handle.type = Types::reflection.NativeHandle;
		handle.instance = (uint8_t*)result;
		VM_Push(thread, &handle);
		CHECKED(GC_Construct(thread, Types::reflection.Module, 1, nullptr));
	}
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION