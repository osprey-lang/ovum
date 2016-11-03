#pragma once

#include <exception>
#include "../vm.h"
#include "../../inc/ovum_string.h"
#include "../../inc/ovum_module.h"
#include "globalmember.h"
#include "modulefile.h"
#include "membertable.h"
#include "../ee/vm.h"
#include "../util/pathname.h"
#include "../util/stringhash.h"

namespace ovum
{

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

	inline int32_t GetMemberCount() const
	{
		return members.GetCount();
	}

	inline bool GetMemberByIndex(int32_t index, GlobalMember &result) const
	{
		return members.GetByIndex(index, result);
	}

	inline Method *GetMainMethod() const
	{
		return mainMethod;
	}

	inline void *GetStaticState() const
	{
		return staticState;
	}

	inline VM *GetVM() const
	{
		return vm;
	}

	inline GC *GetGC() const
	{
		return vm->GetGC();
	}

	Module *FindModuleRef(String *name) const;

	bool FindMember(String *name, bool includeInternal, GlobalMember &result) const;

	Type *FindType(String *name, bool includeInternal) const;

	Method *FindGlobalFunction(String *name, bool includeInternal) const;

	bool FindConstant(String *name, bool includeInternal, Value &result) const;

	Module *FindModuleRef(Token token) const;

	Type *FindType(Token token) const;

	Method *FindMethod(Token token) const;

	Field *FindField(Token token) const;

	String *FindString(Token token) const;

	void *FindNativeFunction(const char *name);

	void InitStaticState(void *state, StaticStateDeallocator deallocator);

	// See ModuleFinder for details on how modules are located.
	static Module *OpenByName(
		VM *vm,
		String *name,
		ModuleVersion *requiredVersion,
		PartiallyOpenedModulesList &partiallyOpenedModules
	);

	static Module *Open(
		VM *vm,
		const PathName &fileName,
		ModuleVersion *requiredVersion,
		PartiallyOpenedModulesList &partiallyOpenedModules
	);

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(Module);

	// The module's name.
	String *name;
	// The module's version.
	ModuleVersion version;
	// The name of the file from which the module was loaded.
	const PathName fileName;

	// The module's main method.
	Method *mainMethod;

	// Handle to native library
	os::LibraryHandle nativeLib;
	// The module's static state (only used by the native library)
	void *staticState;
	// Deallocation callback for the static state
	StaticStateDeallocator staticStateDeallocator;

	// Debug data attached to the module.
	Box<debug::ModuleDebugData> debugData;

	// The VM instance that the module belongs to
	VM *vm;
	// The module pool that the module belongs to
	ModulePool *pool;

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
	StringHash<GlobalMember> members;

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
	friend class RootSetWalker;
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
