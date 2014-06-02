#pragma once

#ifndef VM__MODULE_INTERNAL_H
#define VM__MODULE_INTERNAL_H

#include <cstdio>
#include <vector>
#include <string>
#include <exception>
#include "ov_vm.internal.h"
#include "membertable.internal.h"
#include "pathname.internal.h"

enum class ModuleMemberFlags : uint32_t
{
	// Mask for extracting the kind of member (type, function or constant).
	KIND     = 0x000f,

	NONE     = 0x0000,

	TYPE     = 0x0001,
	FUNCTION = 0x0002,
	CONSTANT = 0x0003,

	PROTECTION = 0x00f0,
	PUBLIC     = 0x0010,
	INTERNAL   = 0x0020,
};
ENUM_OPS(ModuleMemberFlags, uint32_t);


class ModuleMember
{
public:
	ModuleMemberFlags flags;
	union
	{
		Type   *type;
		Method *function;
		Value   constant;
	};

	inline ModuleMember() :
		flags(ModuleMemberFlags::NONE)
	{ }
	inline ModuleMember(Type *type, bool isInternal) :
		type(type), flags(ModuleMemberFlags::TYPE | (isInternal ? ModuleMemberFlags::INTERNAL : ModuleMemberFlags::PUBLIC))
	{ }
	inline ModuleMember(Method *function, bool isInternal) :
		function(function), flags(ModuleMemberFlags::FUNCTION | (isInternal ? ModuleMemberFlags::INTERNAL : ModuleMemberFlags::PUBLIC))
	{ }
	inline ModuleMember(Value value, bool isInternal) :
		constant(value), flags(ModuleMemberFlags::CONSTANT | (isInternal ? ModuleMemberFlags::INTERNAL : ModuleMemberFlags::PUBLIC))
	{ }
};

enum ModuleMemberId : uint32_t
{
	IDMASK_MEMBERKIND   = 0xff000000u,
	IDMASK_MEMBERINDEX  = 0x00ffffffu,
	IDMASK_CONSTANTDEF  = 0x02000000u,
	IDMASK_FUNCTIONDEF  = 0x04000000u,
	IDMASK_TYPEDEF      = 0x10000000u,
	IDMASK_FIELDDEF     = 0x12000000u,
	IDMASK_METHODDEF    = 0x14000000u,
	IDMASK_STRING       = 0x20000000u,
	IDMASK_MODULEREF    = 0x40000000u,
	IDMASK_FUNCTIONREF  = 0x44000000u,
	IDMASK_TYPEREF      = 0x50000000u,
	IDMASK_FIELDREF     = 0x52000000u,
	IDMASK_METHODREF    = 0x54000000u,
};

class ModuleReader; // Defined in modulereader.internal.h

// Types that are specific to module loading
typedef struct ModuleMeta_S
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
} ModuleMeta;

// And then the actual Module class! Hurrah!
class Module
{
public:
	Module(uint32_t fileFormatVersion, ModuleMeta &meta, const PathName &fileName);
	~Module();

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

	static Module *Find(String *name);
	static Module *Open(const PathName &fileName);
	// Module name resolution for a module named $name is performed by
	// looking for the following files, in the order written:
	//   $startupPath/lib/$name/$name.ovm
	//   $startupPath/lib/$name.ovm
	//   $startupPath/$name/$name.ovm
	//   $startupPath/$name.ovm
	//   $modulePath/$name/$name.ovm
	//   $modulePath/$name.ovm
	// where
	//   $startupPath = VM::vm->startupPath
	//   $modulePath = VM::vm->modulePath
	static Module *OpenByName(String *name);

	NOINLINE static int Init();
	NOINLINE static void Unload();

private:
	class Pool
	{
	private:
		int capacity;
		int length;
		Module **data;

		void Init(int capacity)
		{
			capacity = max(capacity, 4);

			data = new Module*[capacity];
			memset(data, 0, sizeof(Module*) * capacity);

			this->capacity = capacity;
		}

		void Resize()
		{
			int newCap = capacity * 2;

			Module **newData = new Module*[newCap];
			CopyMemoryT(newData, this->data, capacity);

			capacity = newCap;
			delete[] this->data;
			this->data = newData;
		}

	public:
		inline Pool() : capacity(0), length(0), data(nullptr)
		{
			Init(0);
		}
		inline Pool(int capacity) : capacity(0), length(0), data(nullptr)
		{
			Init(capacity);
		}
		inline ~Pool()
		{
			for (int i = 0; i < length; i++)
				delete data[i];
			delete[] data;
		}

		inline int GetLength() { return length; }

		inline Module *Get(int index)
		{
			return data[index];
		}
		inline Module *Get(String *name)
		{
			for (int i = 0; i < length; i++)
				if (String_Equals(data[i]->name, name))
					return data[i];
			return nullptr;
		}

		inline void Set(int index, Module *value)
		{
			data[index] = value;
		}

		inline int Add(Module *value)
		{
			if (length == capacity)
				Resize();
			data[length++] = value;
			return length;
		}
	};

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

public:
	String *name;
	ModuleVersion version;
	const PathName fileName;

private:
	bool fullyOpened; // Set to true when the module file has been fully loaded
	                  // If a module depends on another module with this set to false,
	                  // then there's a circular dependency issue.

	// The version number of the file format that the module was saved with.
	uint32_t fileFormatVersion;

	uint32_t methodStart; // The start offset of the method block in the file (set to 0 after opening)
	Method *mainMethod;

	HMODULE nativeLib; // Handle to native library (null if not loaded)

	debug::ModuleDebugData *debugData;

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

	void LoadNativeLibrary(String *nativeFileName, const PathName &path);
	void *FindNativeEntryPoint(const char *name) const;
	void FreeNativeLibrary();

	static Pool *loadedModules;
	typedef void (*NativeModuleMain)(ModuleHandle module);
	static const char *const NativeModuleIniterName;

private:
	static bool FileExists(const PathName &path);

	static void VerifyMagicNumber(ModuleReader &reader);

	static void ReadModuleMeta(ModuleReader &reader, ModuleMeta &target);

	static void ReadVersion(ModuleReader &reader, ModuleVersion &target);

	static void ReadStringTable(ModuleReader &reader, Module *module);

	// Reads the module reference table and opens all dependent modules.
	// Also initializes the moduleRefs table (and moduleRefCount).
	static void ReadModuleRefs(ModuleReader &reader, Module *module);
	static void ReadTypeRefs(ModuleReader &reader, Module *module);
	static void ReadFunctionRefs(ModuleReader &reader, Module *module);
	static void ReadFieldRefs(ModuleReader &reader, Module *module);
	static void ReadMethodRefs(ModuleReader &reader, Module *module);

	static void ReadTypeDefs(ModuleReader &reader, Module *module);
	static void ReadFunctionDefs(ModuleReader &reader, Module *module);
	static void ReadConstantDefs(ModuleReader &reader, Module *module, int32_t headerConstantCount);

	static Type *ReadSingleType(ModuleReader &reader, Module *module, const TokenId typeId, std::vector<FieldConstData> &unresolvedConstants);
	static void ReadFields(ModuleReader &reader, Module *targetModule, Type *targetType, std::vector<FieldConstData> &unresolvedConstants);
	static void ReadMethods(ModuleReader &reader, Module *targetModule, Type *targetType);
	static void ReadProperties(ModuleReader &reader, Module *targetModule, Type *targetType);
	static void ReadOperators(ModuleReader &reader, Module *targetModule, Type *targetType);

	static void SetConstantFieldValue(ModuleReader &reader, Module *module, Field *field, Type *constantType, const int64_t value);

	static Method *ReadSingleMethod(ModuleReader &reader, Module *module);
	static Method::TryBlock *ReadTryBlocks(ModuleReader &reader, Module *targetModule, int32_t &tryCount);

	static void TryRegisterStandardType(Type *type, Module *fromModule, ModuleReader &reader);

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

	enum FileMethodFlags : uint32_t
	{
		FM_PUBLIC    = 0x01,
		FM_PRIVATE   = 0x02,
		FM_PROTECTED = 0x04,
		FM_INSTANCE  = 0x08,
		FM_CTOR      = 0x10,
		FM_IMPL      = 0x20,
	};
	enum OverloadFlags : uint32_t
	{
		OV_VAREND      = 0x01,
		OV_VARSTART    = 0x02,
		OV_NATIVE      = 0x04,
		OV_SHORTHEADER = 0x08,
		OV_VIRTUAL     = 0x10,
		OV_ABSTRACT    = 0x20,
	};
	enum ParamFlags : uint16_t
	{
		PF_BY_REF = 0x0001,
	};
	enum FieldFlags : uint32_t
	{
		FIELD_PUBLIC    = 0x01,
		FIELD_PRIVATE   = 0x02,
		FIELD_PROTECTED = 0x04,
		FIELD_INSTANCE  = 0x08,
		FIELD_HASVALUE  = 0x10,
	};
	enum ConstantFlags : uint32_t
	{
		CONST_PUBLIC  = 0x01,
		CONST_PRIVATE = 0x02,
	};

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

#endif // VM__MODULE_INTERNAL_H