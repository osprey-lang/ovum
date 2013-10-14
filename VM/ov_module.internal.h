#pragma once

#ifndef VM__MODULE_INTERNAL_H
#define VM__MODULE_INTERNAL_H

#include <cstdio>
#include <iosfwd>
#include <fstream>
//#include <vector>
#include "ov_vm.internal.h"
#include "ov_type.internal.h"

// Forward declarations
//class Type;
//class Member;
//class Method;
//class Field;
//class Property;


enum class ModuleMemberFlags : uint16_t
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
ENUM_OPS(ModuleMemberFlags, uint16_t);


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

	inline ModuleMember()
		: constant(NULL_VALUE), flags(ModuleMemberFlags::NONE)
	{ }
	inline ModuleMember(Type *type, bool isInternal)
		: type(type), flags(ModuleMemberFlags::TYPE | (isInternal ? ModuleMemberFlags::INTERNAL : ModuleMemberFlags::PUBLIC))
	{ }
	inline ModuleMember(Method *function, bool isInternal)
		: function(function), flags(ModuleMemberFlags::FUNCTION | (isInternal ? ModuleMemberFlags::INTERNAL : ModuleMemberFlags::PUBLIC))
	{ }
	inline ModuleMember(Value value, bool isInternal)
		: constant(value), flags(ModuleMemberFlags::CONSTANT | (isInternal ? ModuleMemberFlags::INTERNAL : ModuleMemberFlags::PUBLIC))
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
typedef uint32_t TokenId;


class ModuleReader
{
public:
	std::ifstream stream;
	const wchar_t *fileName;

	inline ModuleReader(const wchar_t *fileName)
		: fileName(fileName), stream()
	{
		using namespace std;
		stream.exceptions(ios::failbit | ios::eofbit | ios::badbit);
		stream.open(fileName, ios::binary | ios::in);
	}

	inline ~ModuleReader()
	{
		if (stream.is_open())
			stream.close();
	}

	template<class T>
	inline ModuleReader &Read(T *dest, std::streamsize count)
	{
		stream.read((char*)dest, count * sizeof(T));
		return *this;
	}

	inline int8_t ReadInt8()
	{
		int8_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(int8_t));
		return target;
	}

	inline uint8_t ReadUInt8()
	{
		uint8_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(uint8_t));
		return target;
	}

	// All the reading functions below assume the system is little-endian.
	// This will be changed at an unspecified later date.

	inline int16_t ReadInt16()
	{
		int16_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(int16_t));
		return target;
	}

	inline uint16_t ReadUInt16()
	{
		uint16_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(uint16_t));
		return target;
	}

	inline int32_t ReadInt32()
	{
		int32_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(int32_t));
		return target;
	}

	inline uint32_t ReadUInt32()
	{
		uint32_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(uint32_t));
		return target;
	}

	inline int64_t ReadInt64()
	{
		int64_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(int64_t));
		return target;
	}

	inline uint64_t ReadUInt64()
	{
		uint64_t target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(uint64_t));
		return target;
	}

	inline TokenId ReadToken()
	{
		TokenId target;
		stream.read(reinterpret_cast<char*>(&target), sizeof(TokenId));
		return target;
	}

	inline void SkipCollection()
	{
		using namespace std;
		uint32_t size = ReadUInt32();
		stream.seekg(size, ios::cur);
	}

	String *ReadString();

	String *ReadStringOrNull();

	char *ReadCString();

private:
	String *ReadShortString(const int32_t length);

	String *ReadLongString(const int32_t length);

	static const int MaxShortStringLength = 128;
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
		this->capacity = capacity;
		if (capacity != 0)
			this->entries = new T[capacity];
	}

	inline void Add(const T item)
	{
		entries[length] = item;
		length++;
	}

	inline void DeleteEntries()
	{
		for (int32_t i = 0; i < length; i++)
			delete entries[i];
	}

	inline void FreeEntries()
	{
		for (int32_t i = 0; i < length; i++)
			free(entries[i]);
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
#ifdef PRINT_DEBUG_INFO
		std::wcout << L"Destroying member table" << std::endl;
#endif
		if (this->entries)
			delete[] this->entries;
#ifdef PRINT_DEBUG_INFO
		std::wcout << L"Finished destroying member table" << std::endl;
#endif
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
	Module(ModuleMeta &meta);
	~Module();

	String *name;
	ModuleVersion version;

	const Type *FindType(String *name, bool includeInternal) const;
	Method     *FindGlobalFunction(String *name, bool includeInternal) const;
	const bool  FindConstant(String *name, bool includeInternal, Value &result) const;

	Module     *FindModuleRef(TokenId token) const;
	const Type *FindType(TokenId token) const;
	Method     *FindMethod(TokenId token) const;
	Field      *FindField(TokenId token) const;
	String     *FindString(TokenId token) const;

	Method     *GetMainMethod() const;

	static Module *Find(String *name);
	static Module *Open(const wchar_t *fileName);
	static Module *OpenByName(String *name);

	static void Init();
	static void Unload();

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
#if !NDEBUG
			memset(data, 0, sizeof(Module*) * capacity);
#endif
			this->capacity = capacity;
		}

		void Resize()
		{
			int newCap = capacity * 2;

			Module **newData = new Module*[newCap];
			CopyMemoryT(newData, data, capacity);

			capacity = newCap;
			delete[] data;
			data = newData;
		}

	public:
		inline Pool() : length(0)
		{
			Init(0);
		}
		inline Pool(int capacity) : length(0)
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

private:
	// Represents a T* during module loading.
	// If the module loading fails, or if the method is left without
	// using the value, then the destructor cleans up the memory.
	// If UseValue() has been called on the Temp value, then the value
	// is NOT deallocated in the destructor.
	//
	// If UseFree = true, then the memory is deallocated with free();
	// otherwise, 'delete' is used.
	template<class T, bool UseFree = false>
	class Temp
	{
	private:
		bool isValueUsed;
		T *value;

	public:
		inline Temp()
			: isValueUsed(false), value(nullptr)
		{ }
		inline Temp(T *value)
			: isValueUsed(false), value(value)
		{ }

		inline ~Temp()
		{
			if (!isValueUsed && value != nullptr)
			{
				if (UseFree)
					free(value);
				else
					delete value;
				value = nullptr;
			}
		}

		inline T *GetValue() const
		{
			return value;
		}

		inline T *UseValue()
		{
			isValueUsed = true;
			return value;
		}
	};

	template<class T>
	class TempArr
	{
	private:
		bool isValueUsed;
		T *value;

	public:
		inline TempArr()
			: value(nullptr), isValueUsed(false)
		{ }
		inline TempArr(T *value)
			: value(value), isValueUsed(false)
		{ }

		inline ~TempArr()
		{
			if (!isValueUsed && value != nullptr)
			{
				delete[] value;
				value = nullptr;
			}
		}

		inline T *GetValue() const
		{
			return value;
		}

		inline T *UseValue()
		{
			isValueUsed = true;
			return value;
		}
	};

	bool fullyOpened; // Set to true when the module file has been fully loaded
	                  // If a module depends on another module with this set to false,
	                  // then there's a circular dependency issue.

	MemberTable<Type  *> types;     // Types defined in the module
	MemberTable<Method*> functions; // Global functions defined in the module
	MemberTable<Value  > constants; // Global constants defined in the module
	MemberTable<Field *> fields;    // Fields, both instance and static
	MemberTable<Method*> methods;   // Class methods defined in the module
	MemberTable<String*> strings;   // String table
	StringHash<ModuleMember> members; // All global members defined in the module, indexed by name.

	MemberTable<Module*> moduleRefs;   // Module references
	MemberTable<Type  *> typeRefs;     // Type references
	MemberTable<Method*> functionRefs; // Global function references
	MemberTable<Field *> fieldRefs;    // Field references
	MemberTable<Method*> methodRefs;   // Class method references

	uint32_t methodStart; // The start offset of the method block in the file (set to 0 after opening)
	Method *mainMethod;

	HMODULE nativeLib; // Handle to native library (null if not loaded)

	void LoadNativeLibrary(String *nativeFileName, const wchar_t *path);
	void FreeNativeLibrary();

	static Pool *loadedModules;

private:
	//static void ThrowFileNotFound(const wchar_t *fileName);

	static void VerifyMagicNumber(ModuleReader &reader);

	static void ReadModuleMeta(ModuleReader &reader, ModuleMeta &target);

	static void ReadVersion(ModuleReader &reader, ModuleVersion &target);

	static void ReadStringTable(ModuleReader &reader, Temp<Module> &target);

	// Reads the module reference table and opens all dependent modules.
	// Also initializes the moduleRefs table (and moduleRefCount).
	static void ReadModuleRefs(ModuleReader &reader, Temp<Module> &target);
	static void ReadTypeRefs(ModuleReader &reader, Temp<Module> &target);
	static void ReadFunctionRefs(ModuleReader &reader, Temp<Module> &target);
	static void ReadFieldRefs(ModuleReader &reader, Temp<Module> &target);
	static void ReadMethodRefs(ModuleReader &reader, Temp<Module> &target);

	static void ReadTypeDefs(ModuleReader &reader, Temp<Module> &target);
	static void ReadFunctionDefs(ModuleReader &reader, Temp<Module> &target);
	static void ReadConstantDefs(ModuleReader &reader, Temp<Module> &target);

	static Type *ReadSingleType(ModuleReader &reader, Temp<Module> &target, const TokenId typeId);
	static void ReadFields(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType);
	static void ReadMethods(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType);
	static void ReadProperties(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType);
	static void ReadOperators(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType);

	static Method *ReadSingleMethod(ModuleReader &reader, Temp<Module> &target);
	static Method::TryBlock *ReadTryBlocks(ModuleReader &reader, Temp<Module> &targetModule, int32_t &tryCount);

	static void TryRegisterStandardType(Type *type, Temp<Module> &fromModule, ModuleReader &reader);

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

	friend class ModuleReader;
	friend class GC;
};

// Recover a Module pointer from a ModuleHandle.
//#define _M(mh)	reinterpret_cast<Module*>(mh)

class ModuleLoadException : public std::exception
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