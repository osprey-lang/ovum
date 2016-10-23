#pragma once

#include <cstdio>
#include <vector>
#include <exception>
#include "../vm.h"
#include "../../inc/ov_string.h"
#include "../../inc/ov_module.h"
#include "modulefile.h"
#include "membertable.h"
#include "../ee/vm.h"
#include "../util/pathname.h"
#include "../util/stringhash.h"

namespace ovum
{

class ModuleMember
{
public:
	ModuleMemberFlags flags;
	String *name;
	union
	{
		Type   *type;
		Method *function;
		Value   constant;
	};

	inline ModuleMember() :
		flags(ModuleMemberFlags::NONE), name(nullptr)
	{ }
	ModuleMember(Type *type, bool isInternal);
	ModuleMember(Method *function, bool isInternal);
	ModuleMember(String *name, Value value, bool isInternal);
};

struct ModuleParams
{
	String *name; // The name of the module
	ModuleVersion version;
	// Type count + function count + constant count
	int32_t globalMemberCount;
};

// And then the actual Module class! Hurrah!
class Module
{
public:
	Module(VM *vm, const PathName &fileName, ModuleParams &params);
	~Module();

	Module *FindModuleRef(String *name) const;
	bool    FindMember(String *name, bool includeInternal, ModuleMember &result) const;
	Type   *FindType(String *name, bool includeInternal) const;
	Method *FindGlobalFunction(String *name, bool includeInternal) const;
	bool    FindConstant(String *name, bool includeInternal, Value &result) const;

	Module *FindModuleRef(Token token) const;
	Type   *FindType(Token token) const;
	Method *FindMethod(Token token) const;
	Field  *FindField(Token token) const;
	String *FindString(Token token) const;

	Method *GetMainMethod() const;

	void   *FindNativeFunction(const char *name);

	// See ModuleFinder for details on how modules are located.
	static Module *OpenByName(VM *vm, String *name, ModuleVersion *requiredVersion);
	static Module *Open(VM *vm, const PathName &fileName, ModuleVersion *requiredVersion);

private:
	String *name;
	ModuleVersion version;
	const PathName fileName;

	bool fullyOpened; // Set to true when the module file has been fully loaded
	                  // If a module depends on another module with this set to false,
	                  // then there's a circular dependency issue.

	Method *mainMethod;

	os::LibraryHandle nativeLib; // Handle to native library
	void *staticState; // The module's static state (only used by the native library)
	StaticStateDeallocator staticStateDeallocator; // Deallocation callback for the static state

	Box<debug::ModuleDebugData> debugData;

	VM *vm; // The VM instance that the module belongs to
	ModulePool *pool; // The module pool that the module belongs to

	// Types defined in the module
	MemberTable<Box<Type>> types;
	// Global functions defined in the module
	MemberTable<Box<Method>> functions;
	// Fields, both instance and static
	MemberTable<Box<Field>> fields;
	// Class methods defined in the module
	MemberTable<Box<Method>> methods;
	// String table
	MemberTable<String*> strings;
	// All global members defined in the module, indexed by name.
	StringHash<ModuleMember> members;

	// Module references
	MemberTable<Module*> moduleRefs;
	// Type references
	MemberTable<Type*> typeRefs;
	// Global function references
	MemberTable<Method*> functionRefs;
	// Field references
	MemberTable<Field*> fieldRefs;
	// Class method references
	MemberTable<Method*> methodRefs;

public:
	inline String *GetName() const
	{
		return name;
	}

	inline const ModuleVersion &GetVersion() const
	{
		return version;
	}

	inline const PathName &GetFileName() const
	{
		return fileName;
	}

	inline void *GetStaticState() const
	{
		return staticState;
	}

	void InitStaticState(void *state, StaticStateDeallocator deallocator);

	inline int32_t GetMemberCount() const
	{
		return members.GetCount();
	}

	inline bool GetMemberByIndex(int32_t index, ModuleMember &result) const
	{
		return members.GetByIndex(index, result);
	}

	inline VM *GetVM() const
	{
		return vm;
	}

	inline GC *GetGC() const
	{
		return vm->GetGC();
	}

private:
	void LoadNativeLibrary(String *nativeFileName, const PathName &path);
	void *FindNativeEntryPoint(const char *name);
	void FreeNativeLibrary();

	void TryRegisterStandardType(Type *type);

	static inline int CompareVersion(const ModuleVersion &a, const ModuleVersion &b)
	{
		if (a.major != b.major)
			return a.major < b.major ? -1 : 1;

		if (a.minor != b.minor)
			return a.minor < b.minor ? -1 : 1;

		if (a.patch != b.patch)
			return a.patch < b.patch ? -1 : 1;

		return 0; // equal
	}

	friend class ModulePool;
	friend class ModuleReader;
	friend class GC;
	friend class debug::ModuleDebugData;
	friend class debug::DebugSymbolsReader;
};

class ModuleLoadException : public std::exception
{
private:
	const PathName fileName;

public:
	inline ModuleLoadException(const PathName &fileName)
		: fileName(fileName), exception("Module could not be loaded")
	{ }
	inline ModuleLoadException(const PathName &fileName, const char *message)
		: fileName(fileName), exception(message)
	{ }

	inline ModuleLoadException(const pathchar_t *fileName, const char *message) :
		fileName(fileName), exception(message)
	{ }

	inline const PathName &GetFileName() const throw()
	{
		return this->fileName;
	}
};

class ModuleIOException : public std::exception
{
public:
	inline ModuleIOException(const char *const what) :
		exception(what)
	{ }
};

} // namespace ovum
