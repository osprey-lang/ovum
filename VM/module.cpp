#include <shlwapi.h>
#include <iostream>
#include <sstream>
#include "ov_module.internal.h"
#include "ov_stringbuffer.internal.h"

namespace
{
	LitString<4> _stdModuleName = { 4, 0, STR_STATIC, 'a','v','e','s',0 };
}
String *stdModuleName = _S(_stdModuleName);

vector<Module*> Module::loadedModules = vector<Module*>(0);

namespace module_file
{
	// The magic number that must be present in all Ovum modules.
	const char magicNumber[] = { 'O', 'V', 'M', 'M' };

	// The start of the "real" data in the module.
	const unsigned int dataStart = 16;
}

String *ModuleReader::ReadString()
{
	int32_t length = ReadInt32();

	MutableString *output = (MutableString*)malloc(sizeof(String) + length * sizeof(uchar));

	output->length   = length;
	output->hashCode = 0;
	output->flags    = STR_STATIC;

	// Note: the module file does NOT include a terminating \0!
	Read(&output->firstChar, length);
	(&output->firstChar)[length] = '\0'; // So we need to add it ourselves

	return (String*)output;
}

String *ModuleReader::ReadStringOrNull()
{
	int32_t length = ReadInt32();

	if (length == 0)
		return nullptr;

	MutableString *output = (MutableString*)malloc(sizeof(String) + length * sizeof(uchar));

	output->length   = length;
	output->hashCode = 0;
	output->flags    = STR_STATIC;

	// Note: the module file does NOT include a terminating \0!
	Read(&output->firstChar, length);
	(&output->firstChar)[length] = '\0'; // So we need to add it ourselves

	return (String*)output;
}

Module::Module(ModuleMeta *meta) :
	// This initializer list is kind of silly
	name(meta->name), version(meta->version), fullyOpened(false),
	// defs
	functions(meta->functionCount),
	types(meta->typeCount),
	fields(meta->fieldCount),
	methods(meta->methodCount),
	strings(0), // for now
	members(meta->functionCount + meta->typeCount + meta->constantCount),
	// refs
	moduleRefs(0),
	functionRefs(0),
	typeRefs(0),
	fieldRefs(0),
	methodRefs(0)
{ }

const Type *Module::FindType(String *name, bool includeInternal) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return nullptr;

	if (!includeInternal && (member.flags & MMEM_PROTECTION) == MMEM_INTERNAL
		||
		(member.flags & MMEM_KIND) != MMEM_TYPE)
		return nullptr;

	return member.type;
}

Method *Module::FindGlobalFunction(String *name, bool includeInternal) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return nullptr;

	if (!includeInternal && (member.flags & MMEM_PROTECTION) == MMEM_INTERNAL
		||
		(member.flags & MMEM_KIND) != MMEM_FUNCTION)
		return nullptr;

	return member.function;
}

const bool Module::FindConstant(String *name, bool includeInternal, Value &result) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return false;

	if (!includeInternal && (member.flags & MMEM_PROTECTION) == MMEM_INTERNAL
		||
		(member.flags & MMEM_KIND) != MMEM_CONSTANT)
		return false;

	result = member.constant;
	return true;
}

#define TOKEN_INDEX(tok)    (((tok) & IDMASK_MEMBERINDEX) - 1)

Module *Module::FindModuleRef(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_MODULEREF);

	return moduleRefs[TOKEN_INDEX(token)];
}

Method *Module::FindGlobalFunction(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_FUNCTIONDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_FUNCTIONREF);

	if (token & IDMASK_FUNCTIONDEF)
		return functions[TOKEN_INDEX(token)];
	else if (token & IDMASK_FUNCTIONREF)
		return functionRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

const Type *Module::FindType(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_TYPEDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_TYPEREF);

	if (token & IDMASK_TYPEDEF)
		return types[TOKEN_INDEX(token)];
	else if (token & IDMASK_TYPEREF)
		return typeRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

Method *Module::FindMethod(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_METHODDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_METHODREF);

	if (token & IDMASK_METHODDEF)
		return methods[TOKEN_INDEX(token)];
	else if (token & IDMASK_METHODREF)
		return methodRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

Field *Module::FindField(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_FIELDDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_FIELDREF);

	if (token & IDMASK_FIELDDEF)
		return fields[TOKEN_INDEX(token)];
	else if (token & IDMASK_FIELDREF)
		return fieldRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

String *Module::FindString(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_STRING);

	return strings[TOKEN_INDEX(token)];
}


Module *Module::Find(String *name)
{
	using namespace std;

	vector<Module>::size_type len = Module::loadedModules.size();
	for (unsigned i = 0; i < len; i++)
	{
		Module *mod = Module::loadedModules[i];
		if (String_Equals(mod->name, name))
			return mod;
	}

	return nullptr;
}


Module *Module::Open(const wchar_t *fileName)
{
	DWORD attrs = GetFileAttributesW(fileName);
	if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
		throw ModuleLoadException(fileName, "Module file does not exist.");

	Module *output;

	ModuleReader reader(fileName); // This SHOULD not fail. but it's C++, so who knows.
	try
	{
		VerifyMagicNumber(reader);

		reader.stream.seekg(module_file::dataStart, ios::beg);

		ModuleMeta meta;
		ReadModuleMeta(reader, meta);

		// ReadModuleMeta gives us just enough information to initialize the output module
		// and add it to the list of loaded modules.
		// It's not fully loaded yet, but we add it specifically so that we can detect
		// circular dependencies.
		output = new Module(&meta);
		loadedModules.push_back(output);

		if (meta.nativeLib)
			output->LoadNativeLibrary(meta.nativeLib, fileName);

		// And these must be called in exactly this order!
		ReadModuleRefs(reader, output);
		ReadTypeRefs(reader, output);
		ReadFunctionRefs(reader, output);
		ReadFieldRefs(reader, output);
		ReadMethodRefs(reader, output);

		ReadStringTable(reader, output);

		ReadTypeDefs(reader, output);
		ReadFunctionDefs(reader, output);
		reader.SkipCollection(); // Skip constant defs (they're a compile-time feature)

		TokenId mainMethodId = reader.ReadToken();
		if ((mainMethodId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF &&
			(mainMethodId & IDMASK_MEMBERKIND) != IDMASK_FUNCTIONDEF)
			throw ModuleLoadException(reader.fileName, "Main method token ID must be a MethodDef or FunctionDef.");

		Method *mainMethod = output->FindMethod(mainMethodId);
		if (mainMethod == nullptr)
			throw ModuleLoadException(reader.fileName, "Unresolved main method token ID.");
		if (mainMethod->flags & M_INSTANCE)
			throw ModuleLoadException(reader.fileName, "Main method cannot be an instance method.");

		output->mainMethod = mainMethod;
	}
	catch (std::ios_base::failure &ioError)
	{
		bool eof = reader.stream.eof();

		if (reader.stream.is_open())
			reader.stream.close();

		if (eof) // unexpected EOF
			throw ModuleLoadException(fileName, "Unexpected end of file.");
		throw ModuleLoadException(fileName, "Unspecified module load error.");
	}
	if (reader.stream.is_open())
		reader.stream.close();

	throw ModuleLoadException(fileName, "Function not fully implemented yet.");

	return output;
}

Module *Module::OpenByName(String *name)
{
	Module *mod;
	if (mod = Find(name))
		return mod;

	StringBuffer moduleFileName(nullptr, max(vmState.startupPath->length, vmState.modulePath->length) + name->length + 16);

	const int pathCount = 2;
	String *paths[pathCount] = { vmState.startupPath, vmState.modulePath };

	wchar_t *filePath;

	for (int i = 0; i < pathCount; i++)
	{
		moduleFileName.Clear();

		moduleFileName.Append(nullptr, paths[i]);
		if (!moduleFileName.EndsWith('\\'))
			moduleFileName.Append(nullptr, (uchar)'\\');
		moduleFileName.Append(nullptr, name);
		moduleFileName.Append(nullptr, 4, ".ovm");

		const int filePathLength = moduleFileName.ToWString(nullptr);
		filePath = new wchar_t[filePathLength];
		moduleFileName.ToWString(filePath);

		DWORD attrs = GetFileAttributesW(filePath);
		if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
			break; // we've found our file! \o/

		delete[] filePath; // Clean up~
		filePath = nullptr;
	}

	if (filePath == nullptr) // not found
	{
		const int moduleNameLength = String_ToWString(nullptr, name);
		wchar_t *wname = new wchar_t[moduleNameLength];
		String_ToWString(wname, name);
		throw ModuleLoadException(wname, "Could not locate the module file.");
	}

	if (vmState.verbose)
	{
		wcout << L"Loading module '";
		VM_Print(name);
		wcout << L"' from file '" << filePath << L'\'' << endl;
	}

	Module *output = Open(filePath);

	if (vmState.verbose)
	{
		wcout << L"Successfully loaded module '";
		VM_Print(name);
		wcout << L"'." << endl;
	}

	// Mustn't forget to clean up after ourselves!
	delete[] filePath;
	moduleFileName.~StringBuffer();

	return output;
}

void Module::LoadNativeLibrary(String *nativeFileName, const wchar_t *path)
{
	// Native library files are ALWAYS loaded from the same folder
	// as the module file. Immer & mindig. 'path' contains the full
	// path and file name of the module file, so we strip the module
	// file name and append nativeFileName! Simple!
	size_t pathLen = wcslen(path);
	wchar_t *pathBuf = new wchar_t[max(MAX_PATH, pathLen) + 1];
	pathBuf[pathLen] = L'\0';
	CopyMemoryT(pathBuf, path, pathLen);
	PathRemoveFileSpecW(pathBuf); // get the actual path!

	const int fileNameWLen = String_ToWString(nullptr, nativeFileName);
	wchar_t *fileNameW = new wchar_t[fileNameWLen];
	String_ToWString(fileNameW, nativeFileName);

	PathAppendW(pathBuf, PathFindFileNameW(fileNameW));

	// pathBuf should now contain a full path to the native module
	this->nativeLib = LoadLibraryW(pathBuf);

	delete[] fileNameW; // ain't be needed no more
	delete[] pathBuf; // also not needed anymore now

	// If this->nativeLib is null, then the library could not be loaded.
	if (!this->nativeLib)
		throw ModuleLoadException(path, "Could not load native library file.");
}

void Module::VerifyMagicNumber(ModuleReader &reader)
{
	char magicNumber[4];
	reader.Read(magicNumber, 4);
	for (int i = 0; i < 4; i++)
		if (magicNumber[i] != module_file::magicNumber[i])
			throw ModuleLoadException(reader.fileName, "Invalid magic number in reader.");
}

void Module::ReadModuleMeta(ModuleReader &reader, ModuleMeta &target)
{
	target.name = reader.ReadString(); // name
	ReadVersion(reader, target.version); // version

	// String map (skip)
	reader.SkipCollection();

	target.nativeLib = reader.ReadStringOrNull(); // nativeLib (nullptr if absent)

	target.typeCount     = reader.ReadInt32();  // typeCount
	target.functionCount = reader.ReadInt32();  // functionCount
	target.constantCount = reader.ReadInt32();  // constantCount
	target.fieldCount    = reader.ReadInt32();  // fieldCount
	target.methodCount   = reader.ReadInt32();  // methodCount
	target.methodStart   = reader.ReadUInt32(); // methodStart
}

void Module::ReadVersion(ModuleReader &reader, ModuleVersion &target)
{
	target.major    = reader.ReadInt32();
	target.minor    = reader.ReadInt32();
	target.build    = reader.ReadInt32();
	target.revision = reader.ReadInt32();
}

// Some macros to help check the position of the file stream,
// for collections that contain a size. Note that this also
// includes an if statement that makes the code do nothing if
// the size is 0.
// (Semicolons intentionally left out to force a "missing ';'" error if you forget it.)
#define CHECKPOS_BEFORE() \
	uint32_t size = reader.ReadUInt32(); /* The size of the rest of the data */ \
	if (size != 0) { \
		ios::pos_type posBefore = reader.stream.tellg()

#define CHECKPOS_AFTER_(tbl) \
		ios::pos_type posAfter = reader.stream.tellg(); \
		if (posBefore + (ios::pos_type)size != posAfter) \
			throw ModuleLoadException(reader.fileName, "The actual size of the " #tbl " table did not match the expected size."); \
	}
#define CHECKPOS_AFTER(tbl) CHECKPOS_AFTER_(tbl)

void Module::ReadModuleRefs(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->moduleRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->moduleRefs.GetNextId(IDMASK_MODULEREF))
			throw ModuleLoadException(reader.fileName, "Invalid ModuleRef token ID.");
		// Module reference has a name followed by a minimum version
		String *modName = reader.ReadString();
		ModuleVersion minVer;
		ReadVersion(reader, minVer);

		Module *ref = OpenByName(modName);
		if (!ref->fullyOpened)
			throw ModuleLoadException(reader.fileName, "Circular dependency detected.");
		if (ModuleVersion::Compare(ref->version, minVer) < 0)
			throw ModuleLoadException(reader.fileName, "Dependent module has insufficient version.");

		target->moduleRefs.Add(ref);
	}

	CHECKPOS_AFTER(ModuleRef);
}

void Module::ReadTypeRefs(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->typeRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->typeRefs.GetNextId(IDMASK_TYPEREF))
			throw ModuleLoadException(reader.fileName, "Invalid TypeRef token ID.");
		// Type reference has a name followed by a ModuleRef ID.
		String *typeName = reader.ReadString();
		TokenId modRef = reader.ReadToken();

		const Module *owner = target->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.fileName, "Unresolved ModuleRef token in TypeRef.");

		const Type *type = owner->FindType(typeName, false);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef.");

		target->typeRefs.Add(const_cast<Type*>(type));
	}

	CHECKPOS_AFTER(TypeRef);
}

void Module::ReadFunctionRefs(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->functionRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->functionRefs.GetNextId(IDMASK_FUNCTIONREF))
			throw ModuleLoadException(reader.fileName, "Invalid FunctionRef token ID.");
		// Function reference has a name followed by a ModuleRef ID
		String *funcName = reader.ReadString();
		TokenId modRef = reader.ReadToken();

		const Module *owner = target->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.fileName, "Invalid module token ID in FunctionRef.");

		Method *func = owner->FindGlobalFunction(funcName, false);
		if (!func)
			throw ModuleLoadException(reader.fileName, "Unresolved FunctionRef.");

		target->functionRefs.Add(func);
	}

	CHECKPOS_AFTER(FunctionRef);
}

void Module::ReadFieldRefs(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->fieldRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->fieldRefs.GetNextId(IDMASK_FIELDREF))
			throw ModuleLoadException(reader.fileName, "Invalid FieldRef token ID.");
		// Field reference has a name followed by a TypeRef ID.
		String *fieldName = reader.ReadString();
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.fileName, "FieldRef must contain a TypeRef.");

		const Type *type = target->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef token in FieldRef.");

		Member *member = type->GetMember(fieldName);
		if (!member)
			throw ModuleLoadException(reader.fileName, "Unresolved FieldRef.");
		if (!(member->flags & M_FIELD))
			throw ModuleLoadException(reader.fileName, "FieldRef does not refer to a field.");

		target->fieldRefs.Add((Field*)member);
	}

	CHECKPOS_AFTER(FieldRef);
}

void Module::ReadMethodRefs(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->methodRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->methodRefs.GetNextId(IDMASK_METHODREF))
			throw ModuleLoadException(reader.fileName, "Invalid MethodRef token ID.");
		// Method reference has a name followed by a TypeRef ID.
		String *methodName = reader.ReadString();
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.fileName, "MethodRef must contain a TypeRef.");

		const Type *type = target->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef token in MethodRef.");

		Member *member = type->GetMember(methodName);
		if (!member)
			throw ModuleLoadException(reader.fileName, "Unresolved MethodRef.");
		if (!(member->flags & M_METHOD))
			throw ModuleLoadException(reader.fileName, "MethodRef does not refer to a method.");

		target->methodRefs.Add((Method*)member);
	}

	CHECKPOS_AFTER(MethodRef);
}

void Module::ReadStringTable(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->strings.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->strings.GetNextId(IDMASK_STRING))
			throw ModuleLoadException(reader.fileName, "Invalid String token ID.");

		String *value = reader.ReadString();
		target->strings.Add(value);
	}

	CHECKPOS_AFTER(String);
}

void Module::ReadTypeDefs(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->types.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->types.GetNextId(IDMASK_TYPEDEF))
			throw new ModuleLoadException(reader.fileName, "Invalid TypeDef token ID.");

		Type *type = ReadSingleType(reader, target, id);
		target->types.Add(type);
	}

	CHECKPOS_AFTER(TypeDef);
}

void Module::ReadFunctionDefs(ModuleReader &reader, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	target->types.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != target->functions.GetNextId(IDMASK_FUNCTIONDEF))
			throw new ModuleLoadException(reader.fileName, "Invalid TypeDef token ID.");

		Method *function = ReadSingleMethod(reader, target);
		target->functions.Add(function);
	}

	CHECKPOS_AFTER(FunctionDef);
}

Type *Module::ReadSingleType(ModuleReader &reader, Module *target, const TokenId typeId)
{
	TypeFlags flags = (TypeFlags)reader.ReadUInt32();
	String *name = reader.ReadString();

	TokenId baseTypeId   = reader.ReadToken();
	TokenId sharedTypeId = reader.ReadToken();

	const Type *baseType;
	if (baseTypeId != 0)
	{
		if (baseTypeId == typeId)
			throw ModuleLoadException(reader.fileName, "A type cannot have itself as its base type.");
		baseType = target->FindType(baseTypeId);
		if (baseType == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve base type ID.");
	}

	const Type *sharedType;
	if (sharedTypeId != 0)
	{
		if ((sharedTypeId & IDMASK_MEMBERKIND) != IDMASK_TYPEDEF)
			throw ModuleLoadException(reader.fileName, "A shared type must be a TypeDef.");
		if (sharedTypeId == typeId)
			throw ModuleLoadException(reader.fileName, "A type cannot have itself as its shared type.");
		sharedType = target->FindType(sharedTypeId);
		if (sharedType == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve shared type ID.");
	}

	const int32_t memberCount = reader.ReadInt32();
	Type *output = new Type(memberCount);
}


// Paper thin API wrapper functions, whoo!

OVUM_API ModuleHandle FindModule(String *name)
{
	return (ModuleHandle)Module::Find(name);
}

OVUM_API TypeHandle Module_FindType(ModuleHandle module, String *name, bool includeInternal)
{
	return _M(module)->FindType(name, includeInternal);
}

OVUM_API MethodHandle Module_FindGlobalFunction(ModuleHandle module, String *name, bool includeInternal)
{
	return _M(module)->FindGlobalFunction(name, includeInternal);
}

OVUM_API const bool Module_FindConstant(ModuleHandle module, String *name, bool includeInternal, Value &result)
{
	return _M(module)->FindConstant(name, includeInternal, result);
}