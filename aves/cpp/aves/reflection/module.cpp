#include "module.h"
#include "type.h"
#include "../../aves_state.h"
#include "../../tempbuffer.h"
#include <ov_module.h>
#include <stddef.h>

using namespace aves;

AVES_API int OVUM_CDECL aves_reflection_Module_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ModuleInst));

	int status__;
	CHECKED(Type_AddNativeField(type, offsetof(ModuleInst,fileName), NativeFieldType::STRING));
	CHECKED(Type_AddNativeField(type, offsetof(ModuleInst,version),  NativeFieldType::VALUE));

retStatus__:
	return status__;
}

int GetMemberSearchFlags(ThreadHandle thread, Value *arg, ModuleMemberFlags *result)
{
	Aves *aves = Aves::Get(thread);

	if (arg->type != aves->aves.reflection.MemberSearchFlags)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::flags); // paramName
		int r = GC_Construct(thread, aves->aves.ArgumentError, 2, nullptr);
		if (r == OVUM_SUCCESS)
			r = VM_Throw(thread);
		return r;
	}

	MemberSearchFlags flags = (MemberSearchFlags)arg->v.integer;

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
	Aves *aves = Aves::Get(thread);

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
			handle.type = aves->aves.reflection.NativeHandle;
			handle.v.instance = (uint8_t*)member->function;
			VM_Push(thread, &handle);
			r = GC_Construct(thread, aves->aves.reflection.Method, 1, nullptr);
		}
		break;
	case ModuleMemberFlags::CONSTANT:
		{
			VM_Push(thread, module);
			VM_PushBool(thread, (member->flags & ModuleMemberFlags::INTERNAL) == ModuleMemberFlags::INTERNAL);
			VM_PushString(thread, member->name);
			VM_Push(thread, &member->constant);
			r = GC_Construct(thread, aves->aves.reflection.GlobalConstant, 4, nullptr);
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
	int status__;
	{
		Value *list = VM_Local(thread, 0);
		VM_PushInt(thread, 5);
		CHECKED(GC_Construct(thread, GetType_List(thread), 1, list));

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
	status__ = OVUM_SUCCESS;
retStatus__:
	return status__;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_new)
{
	Aves *aves = Aves::Get(thread);

	// new(handle)

	if (args[1].type != aves->aves.reflection.NativeHandle)
	{
		VM_PushNull(thread); // message
		VM_PushString(thread, strings::handle); // paramName
		CHECKED(GC_Construct(thread, aves->aves.ArgumentError, 2, nullptr));
		return VM_Throw(thread);
	}

	ModuleInst *inst = THISV.Get<ModuleInst>();
	inst->module = (ModuleHandle)args[1].v.instance;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_handle)
{
	Aves *aves = Aves::Get(thread);

	ModuleInst *inst = THISV.Get<ModuleInst>();

	Value handle;
	handle.type = aves->aves.reflection.NativeHandle;
	handle.v.instance = (uint8_t*)inst->module;
	VM_Push(thread, &handle);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_name)
{
	ModuleInst *inst = THISV.Get<ModuleInst>();
	VM_PushString(thread, Module_GetName(inst->module));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_get_version)
{
	Aves *aves = Aves::Get(thread);

	ModuleInst *inst = THISV.Get<ModuleInst>();

	if (IS_NULL(inst->version))
	{
		ModuleVersion version;
		Module_GetVersion(inst->module, &version);
		VM_PushInt(thread, version.major);
		VM_PushInt(thread, version.minor);
		VM_PushInt(thread, version.patch);
		CHECKED(GC_Construct(thread, aves->aves.Version, 3, &inst->version));
	}

	VM_Push(thread, &inst->version);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_get_fileName)
{
	ModuleInst *inst = THISV.Get<ModuleInst>();

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

	ModuleInst *inst = THISV.Get<ModuleInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

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

	ModuleInst *inst = THISV.Get<ModuleInst>();

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::TYPE));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getFunction)
{
	// getFunction(name, flags)

	ModuleInst *inst = THISV.Get<ModuleInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

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

	ModuleInst *inst = THISV.Get<ModuleInst>();

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::FUNCTION));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getGlobalConstant)
{
	// getGlobalConstant(name, flags)

	ModuleInst *inst = THISV.Get<ModuleInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

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

	ModuleInst *inst = THISV.Get<ModuleInst>();

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::CONSTANT));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getMember)
{
	// getMember(name, flags)

	ModuleInst *inst = THISV.Get<ModuleInst>();

	CHECKED(StringFromValue(thread, args + 1));
	String *name = args[1].v.string;

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

	ModuleInst *inst = THISV.Get<ModuleInst>();

	ModuleMemberFlags flags;
	CHECKED(GetMemberSearchFlags(thread, args + 1, &flags));

	CHECKED(GetAllMembers(thread, inst->module, THISP, flags, ModuleMemberFlags::KIND));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getCurrentModule)
{
	Aves *aves = Aves::Get(thread);

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
	handle.type = aves->aves.reflection.NativeHandle;
	handle.v.instance = (uint8_t*)module;
	VM_Push(thread, &handle);

	CHECKED(GC_Construct(thread, aves->aves.reflection.Module, 1, nullptr));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_getSearchDirectories)
{
	TempBuffer<String*, 16> searchDirs;

	int dirCount = 0;
	do
	{
		CHECKED_MEM(searchDirs.EnsureCapacity(dirCount));
		CHECKED(Module_GetSearchDirectories(
			thread,
			(int)searchDirs.GetCapacity(),
			searchDirs.GetPointer(),
			&dirCount
		));
	} while (dirCount > (int)searchDirs.GetCapacity());

	Value *output = VM_Local(thread, 0);
	VM_PushInt(thread, dirCount); // list capacity
	CHECKED(GC_Construct(thread, GetType_List(thread), 1, output));

	for (int i = 0; i < dirCount; i++)
	{
		Value ignore;
		VM_Push(thread, output);
		VM_PushString(thread, searchDirs[i]);
		CHECKED(VM_InvokeMember(thread, strings::add, 1, &ignore));
	}

	VM_Push(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_reflection_Module_find)
{
	Aves *aves = Aves::Get(thread);

	// find(name is String, version is Version|null)

	PinnedAlias<String> name(args + 0);
	ModuleHandle result;
	if (IS_NULL(args[1]))
		result = FindModule(thread, *name, nullptr);
	else
	{
		ModuleVersion version;
		Value field;
		// major
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadMember(thread, strings::major, &field));
		version.major = static_cast<uint32_t>(field.v.integer);
		// minor
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadMember(thread, strings::minor, &field));
		version.minor = static_cast<uint32_t>(field.v.integer);
		// patch
		VM_Push(thread, args + 1);
		CHECKED(VM_LoadMember(thread, strings::patch, &field));
		version.patch = static_cast<uint32_t>(field.v.integer);

		result = FindModule(thread, *name, &version);
	}

	if (result)
	{
		Value handle;
		handle.type = aves->aves.reflection.NativeHandle;
		handle.v.instance = (uint8_t*)result;
		VM_Push(thread, &handle);
		CHECKED(GC_Construct(thread, aves->aves.reflection.Module, 1, nullptr));
	}
	else
		VM_PushNull(thread);
}
END_NATIVE_FUNCTION
