#pragma once

#ifndef VM__MODULE_INTERNAL_H
#define VM__MODULE_INTERNAL_H

#include <cstdio>
#include <iosfwd>
#include <fstream>
#include <vector>
#include "ov_vm.internal.h"

using namespace std;

// Forward declarations
class Type;
class Member;
class Method;
class Field;
class Property;

extern String *stdModuleName;


TYPED_ENUM(ModuleMemberFlags, uint16_t)
{
	// Mask for extracting the kind of member (type, function or constant).
	MMEM_KIND     = 0x000f,

	MMEM_TYPE     = 0x0001,
	MMEM_FUNCTION = 0x0002,
	MMEM_CONSTANT = 0x0003,

	MMEM_PROTECTION = 0x00f0,
	MMEM_PUBLIC   = 0x0010,
	MMEM_INTERNAL = 0x0020,
};


typedef struct ModuleMember_S
{
	ModuleMemberFlags flags;

	union
	{
		Type   *type;
		Method *function;
		Value   constant;
	};
} ModuleMember;

TYPED_ENUM(ModuleMemberId, uint32_t)
{
	IDMASK_MEMBERKIND   = 0xff000000u,
	IDMASK_MEMBERINDEX  = 0x00ffffffu,
	IDMASK_CONSTANTDEF  = 0x02000000u,
	IDMASK_FUNCTIONDEF  = 0x04000000u,
	IDMASK_TYPEDEF      = 0x10000000u,
	IDMASK_FIELDDEF     = 0x12000000u,
	IDMASK_METHODDEF    = 0x14000000u,
	IDMASK_PROPERTYDEF  = 0x18000000u, // This may be unnecessary
	IDMASK_STRING       = 0x20000000u,
	IDMASK_MODULEREF    = 0x40000000u,
	IDMASK_FUNCTIONREF  = 0x44000000u,
	IDMASK_TYPEREF      = 0x50000000u,
	IDMASK_FIELDREF     = 0x52000000u,
	IDMASK_METHODREF    = 0x54000000u,
};
typedef uint32_t TokenId;


class ModuleReader
{
public:
	ifstream stream;
	const wchar_t *fileName;

	inline ModuleReader(const wchar_t *fileName)
		: fileName(fileName), stream()
	{
		stream.exceptions(ios::failbit | ios::eofbit | ios::badbit);
		stream.open(fileName, ios::binary | ios::in);
	}

	inline ~ModuleReader()
	{
		//if (stream.is_open())
		//	stream.close();
	}

	template<class T>
	inline ModuleReader &Read(T *dest, std::streamsize count)
	{
		stream.read((char*)dest, count * sizeof(T));
		return *this;
	}

	inline int32_t ReadInt32()
	{
		int32_t target;
		stream.read(reinterpret_cast<char*>(&target), 4); // assume little-endian for now
		return target;
	}

	inline uint32_t ReadUInt32()
	{
		uint32_t target;
		stream.read(reinterpret_cast<char*>(&target), 4); // assume little-endian for now
		return target;
	}

	inline TokenId ReadToken()
	{
		TokenId target;
		stream.read(reinterpret_cast<char*>(&target), 4); // assume little-endian for now
		return target;
	}

	inline void SkipCollection()
	{
		uint32_t size = ReadUInt32();
		stream.seekg(size, ios::cur);
	}

	String *ReadString();

	String *ReadStringOrNull();
};

template<>
inline ModuleReader &ModuleReader::Read(char *dest, std::streamsize count)
{
	stream.read(dest, count);
	return *this;
}

typedef struct ModuleVersion_S ModuleVersion;
typedef struct ModuleVersion_S
{
	int32_t major;
	int32_t minor;
	int32_t build;
	int32_t revision;

	static inline const int Compare(ModuleVersion &a, ModuleVersion &b)
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
} ModuleVersion;

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

class Module; // Vorwärts declaration

template<class T>
class MemberTable
{
private:
	int32_t capacity; // The total number of slots
	int32_t length; // The total number of entries
	T *entries;

	inline void Init(const int32_t capacity)
	{
		this->count = capacity;
		if (capacity != 0)
			this->entries = new T[capacity];
	}

	inline void Add(const T item)
	{
		entries[length] = item;
		length++;
	}

public:
	inline MemberTable(const int32_t capacity)
		: length(0)
	{
		Init(capacity);
	}
	inline MemberTable()
	{
		Init(0);
	}

	inline ~MemberTable()
	{
		if (this->capacity != 0)
			delete[] this->entries;
		this->capacity = 0;
	}

	inline T operator[](const uint32_t index) const
	{
		if (index >= length)
			return nullptr; // niet gevonden
		return entries[index];
	}

	inline const int32_t GetLength() const { return length; }
	inline const int32_t GetCapacity() const { return capacity; }
	
	inline bool HasItem(const uint32_t index) const
	{
		return index < length;
	}

	inline TokenId GetNextId(TokenId mask) const
	{
		return (length + 1) | mask;
	}

	friend class Module;
};

// And then the actual Module class! Hurrah!
class Module
{
public:
	Module(ModuleMeta *meta);

	String *name;
	ModuleVersion version;

	const Type *FindType(String *name, bool includeInternal) const;
	Method *FindGlobalFunction(String *name, bool includeInternal) const;
	const bool FindConstant(String *name, bool includeInternal, Value &result) const;

	Module *FindModuleRef(TokenId token) const;
	Method *FindGlobalFunction(TokenId token) const;
	const Type *FindType(TokenId token) const;
	Method *FindMethod(TokenId token) const;
	Field *FindField(TokenId token) const;
	String *FindString(TokenId token) const;

	static Module *Find(String *name);
	static Module *Open(const wchar_t *fileName);
	static Module *OpenByName(String *name);

private:
	bool fullyOpened; // Set to true when the module file has been fully loaded
	                  // If a module depends on another module with this set to false,
	                  // then there's a circular dependency issue.

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

	Method *mainMethod;

	HMODULE nativeLib; // Handle to native library (null if not loaded)

	void LoadNativeLibrary(String *nativeFileName, const wchar_t *path);

	static std::vector<Module*> loadedModules;

	//static void ThrowFileNotFound(const wchar_t *fileName);

	static void VerifyMagicNumber(ModuleReader &reader);

	static void ReadModuleMeta(ModuleReader &reader, ModuleMeta &target);

	static void ReadVersion(ModuleReader &reader, ModuleVersion &target);

	// Reads the module reference table and opens all dependent modules.
	// Also initializes the moduleRefs table (and moduleRefCount).
	static void ReadModuleRefs(ModuleReader &reader, Module *target);
	static void ReadTypeRefs(ModuleReader &reader, Module *target);
	static void ReadFunctionRefs(ModuleReader &reader, Module *target);
	static void ReadFieldRefs(ModuleReader &reader, Module *target);
	static void ReadMethodRefs(ModuleReader &reader, Module *target);

	static void ReadStringTable(ModuleReader &reader, Module *target);

	static void ReadTypeDefs(ModuleReader &reader, Module *target);
	static void ReadFunctionDefs(ModuleReader &reader, Module *target);

	static Type *ReadSingleType(ModuleReader &reader, Module *target, const TokenId typeId);
	static Method *ReadSingleMethod(ModuleReader &reader, Module *target);
};

// Recover a Module pointer from a ModuleHandle.
#define _M(mh)	reinterpret_cast<Module*>(mh)

class ModuleLoadException : public exception
{
private:
	const wchar_t *fileName;

public:
	inline ModuleLoadException(const wchar_t *fileName)
		: fileName(fileName), exception("Module could not be loaded")
	{ }
	inline ModuleLoadException(const wchar_t *fileName, const char *message)
		: fileName(fileName), exception(message)
	{ }

	inline const wchar_t *GetFileName() const throw()
	{
		return this->fileName;
	}
};


#endif // VM__MODULE_INTERNAL_H