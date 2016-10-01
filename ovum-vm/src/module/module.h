#pragma once

#include <cstdio>
#include <vector>
#include <exception>
#include "../vm.h"
#include "../../inc/ov_string.h"
#include "../../inc/ov_module.h"
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

enum ModuleMemberId : uint32_t
{
	IDMASK_MEMBERKIND  = 0xff000000u,
	IDMASK_MEMBERINDEX = 0x00ffffffu,
	IDMASK_CONSTANTDEF = 0x02000000u,
	IDMASK_FUNCTIONDEF = 0x04000000u,
	IDMASK_TYPEDEF     = 0x10000000u,
	IDMASK_FIELDDEF    = 0x12000000u,
	IDMASK_METHODDEF   = 0x14000000u,
	IDMASK_STRING      = 0x20000000u,
	IDMASK_MODULEREF   = 0x40000000u,
	IDMASK_FUNCTIONREF = 0x44000000u,
	IDMASK_TYPEREF     = 0x50000000u,
	IDMASK_FIELDREF    = 0x52000000u,
	IDMASK_METHODREF   = 0x54000000u,
};

// Types that are specific to module loading
struct ModuleMeta
{
	String *name; // The name of the module
	ModuleVersion version;

	String *nativeLib; // The name of the native library file

	int32_t typeCount;
	int32_t functionCount;
	int32_t constantCount;
	int32_t fieldCount;
	int32_t methodCount;
	uint32_t methodStart;
};

// And then the actual Module class! Hurrah!
class Module
{
public:
	Module(uint32_t fileFormatVersion, ModuleMeta &meta, const PathName &fileName, VM *vm);
	~Module();

	Module *FindModuleRef(String *name) const;
	bool    FindMember(String *name, bool includeInternal, ModuleMember &result) const;
	Type   *FindType(String *name, bool includeInternal) const;
	Method *FindGlobalFunction(String *name, bool includeInternal) const;
	bool    FindConstant(String *name, bool includeInternal, Value &result) const;

	Module *FindModuleRef(TokenId token) const;
	Type   *FindType(TokenId token) const;
	Method *FindMethod(TokenId token) const;
	Field  *FindField(TokenId token) const;
	String *FindString(TokenId token) const;

	Method *GetMainMethod() const;

	void   *FindNativeFunction(const char *name);

	// See ModuleFinder for details on how modules are located.
	static Module *OpenByName(VM *vm, String *name, ModuleVersion *requiredVersion);
	static Module *Open(VM *vm, const PathName &fileName, ModuleVersion *requiredVersion);

private:
	class FieldConstData
	{
	public:
		Field *field;
		TokenId typeId;
		int64_t value;

		inline FieldConstData(Field *field, TokenId typeId, int64_t value) :
			field(field), typeId(typeId), value(value)
		{ }
	};

	String *name;
	ModuleVersion version;
	const PathName fileName;

	bool fullyOpened; // Set to true when the module file has been fully loaded
	                  // If a module depends on another module with this set to false,
	                  // then there's a circular dependency issue.

	// The version number of the file format that the module was saved with.
	uint32_t fileFormatVersion;

	uint32_t methodStart; // The start offset of the method block in the file (set to 0 after opening)
	Method *mainMethod;

	os::LibraryHandle nativeLib; // Handle to native library
	void *staticState; // The module's static state (only used by the native library)
	StaticStateDeallocator staticStateDeallocator; // Deallocation callback for the static state

	debug::ModuleDebugData *debugData;

	VM *vm; // The VM instance that the module belongs to
	ModulePool *pool; // The module pool that the module belongs to

	MemberTable<Type  *> types;     // Types defined in the module
	MemberTable<Method*> functions; // Global functions defined in the module
	MemberTable<Field *> fields;    // Fields, both instance and static
	MemberTable<Method*> methods;   // Class methods defined in the module
	MemberTable<String*> strings;   // String table
	StringHash<ModuleMember> members; // All global members defined in the module, indexed by name.

	MemberTable<Module*> moduleRefs;   // Module references
	MemberTable<Type  *> typeRefs;     // Type references
	MemberTable<Method*> functionRefs; // Global function references
	MemberTable<Field *> fieldRefs;    // Field references
	MemberTable<Method*> methodRefs;   // Class method references

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

	static const char *const NativeModuleIniterName;

	void TryRegisterStandardType(Type *type);

	static inline int CompareVersion(const ModuleVersion &a, const ModuleVersion &b)
	{
		if (a.major != b.major)
			return a.major < b.major ? -1 : 1;

		if (a.minor != b.minor)
			return a.minor < b.minor ? -1 : 1;

		if (a.build != b.build)
			return a.build < b.build ? -1 : 1;

		if (a.revision != b.revision)
			return a.revision < b.revision ? -1 : 1;

		return 0; // equal
	}

	typedef void (OVUM_CDECL *NativeModuleMain)(ModuleHandle module);

	friend class ModulePool;
	friend class ModuleReader;
	friend class GC;
	friend class debug::ModuleDebugData;
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
