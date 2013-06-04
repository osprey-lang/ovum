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

String *MFileStream::ReadString()
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

String *MFileStream::ReadStringOrNull()
{
	int32_t length = ReadInt32();

	if (length == 0)
		return NULL;

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
	constants(meta->constantCount),
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
		return NULL;

	if (!includeInternal && (member.flags & MMEM_PROTECTION) == MMEM_INTERNAL
		||
		(member.flags & MMEM_KIND) != MMEM_TYPE)
		return NULL;

	return member.type;
}

Method *Module::FindGlobalFunction(String *name, bool includeInternal) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return NULL;

	if (!includeInternal && (member.flags & MMEM_PROTECTION) == MMEM_INTERNAL
		||
		(member.flags & MMEM_KIND) != MMEM_FUNCTION)
		return NULL;

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

	return NULL; // not found
}

const Type *Module::FindType(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_TYPEDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_TYPEREF);

	if (token & IDMASK_TYPEDEF)
		return types[TOKEN_INDEX(token)];
	else if (token & IDMASK_TYPEREF)
		return typeRefs[TOKEN_INDEX(token)];

	return NULL; // not found
}

Method *Module::FindMethod(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_METHODDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_METHODREF);

	if (token & IDMASK_METHODDEF)
		return methods[TOKEN_INDEX(token)];
	else if (token & IDMASK_METHODREF)
		return methodRefs[TOKEN_INDEX(token)];

	return NULL; // not found
}

Member *Module::FindField(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_FIELDDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_FIELDREF);

	if (token & IDMASK_FIELDDEF)
		return fields[TOKEN_INDEX(token)];
	else if (token & IDMASK_FIELDREF)
		return fieldRefs[TOKEN_INDEX(token)];

	return NULL; // not found
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

	return NULL;
}


Module *Module::Open(const wchar_t *fileName)
{
	DWORD attrs = GetFileAttributesW(fileName);
	if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
		throw ModuleLoadException(fileName, "Module file does not exist.");

	Module *output;

	MFileStream file(fileName); // This SHOULD not fail. but it's C++, so who knows.
	try
	{
		VerifyMagicNumber(file);

		file.stream.seekg(module_file::dataStart, ios::beg);

		ModuleMeta meta;
		ReadModuleMeta(file, meta);

		// ReadModuleMeta gives us just enough information to initialize the output module
		// and add it to the list of loaded modules.
		// It's not fully loaded yet, but we add it specifically so that we can detect
		// circular dependencies.
		output = new Module(&meta);
		loadedModules.push_back(output);

		if (meta.nativeLib)
			output->LoadNativeLibrary(meta.nativeLib, fileName);

		// And these must be called in exactly this order!
		ReadMethodRefs(file, output);
		ReadFunctionRefs(file, output);
		ReadTypeRefs(file, output);
		ReadFieldRefs(file, output);
		ReadMethodRefs(file, output);

		ReadStringTable(file, output);

		// ... blah blah blah ...

		file.stream.close();
	}
	catch (std::ios_base::failure &ioError)
	{
		bool eof = file.stream.eof();

		if (file.stream.is_open())
			file.stream.close();

		if (eof) // unexpected EOF
			throw ModuleLoadException(fileName, "Unexpected end of file.");
		throw ModuleLoadException(fileName, "Unspecified module load error.");
	}

	throw ModuleLoadException(fileName, "Function not fully implemented yet.");

	return output;
}

Module *Module::OpenByName(String *name)
{
	Module *mod;
	if (mod = Find(name))
		return mod;

	StringBuffer moduleFileName(NULL, max(vmState.startupPath->length, vmState.modulePath->length) + name->length + 16);

	const int pathCount = 2;
	String *paths[] = { vmState.startupPath, vmState.modulePath };

	wchar_t *filePath;

	for (int i = 0; i < pathCount; i++)
	{
		moduleFileName.Clear();

		moduleFileName.Append(NULL, paths[i]);
		if (!moduleFileName.EndsWith('\\'))
			moduleFileName.Append(NULL, (uchar)'\\');
		moduleFileName.Append(NULL, name);
		moduleFileName.Append(NULL, 4, ".ovm");

		const int filePathLength = moduleFileName.ToWString(NULL);
		filePath = new wchar_t[filePathLength];
		moduleFileName.ToWString(filePath);

		DWORD attrs = GetFileAttributesW(filePath);
		if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
			break; // we've found our file! \o/

		delete[] filePath; // Clean up~
		filePath = NULL;
	}

	if (filePath == NULL) // not found
	{
		const int moduleNameLength = String_ToWString(NULL, name);
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

	const int fileNameWLen = String_ToWString(NULL, nativeFileName);
	wchar_t *fileNameW = new wchar_t[fileNameWLen];
	String_ToWString(fileNameW, nativeFileName);

	PathAppendW(pathBuf, PathFindFileNameW(fileNameW));

	// pathBuf should now contain a full path to the native module
	this->nativeLib = LoadLibraryW(pathBuf);

	delete[] fileNameW; // ain't be needed no more
	delete[] pathBuf; // also not needed anymore now

	// If this->nativeLib is NULL, then the library could not be loaded.
	if (!this->nativeLib)
		throw ModuleLoadException(path, "Could not load native library file.");
}

void Module::VerifyMagicNumber(MFileStream &file)
{
	char magicNumber[4];
	file.Read(magicNumber, 4);
	for (int i = 0; i < 4; i++)
		if (magicNumber[i] != module_file::magicNumber[i])
			throw ModuleLoadException(file.fileName, "Invalid magic number in file.");
}

void Module::ReadModuleMeta(MFileStream &file, ModuleMeta &target)
{
	target.name = file.ReadString(); // name
	ReadVersion(file, target.version); // version

	// String map (skip)
	file.SkipCollection();

	target.nativeLib = file.ReadStringOrNull(); // nativeLib (NULL if absent)

	target.typeCount = file.ReadInt32();     // typeCount
	target.functionCount = file.ReadInt32(); // functionCount
	target.constantCount = file.ReadInt32(); // constantCount
	target.fieldCount = file.ReadInt32();    // fieldCount
	target.methodCount = file.ReadInt32();   // methodCount
	target.methodStart = file.ReadUInt32();  // methodStart
}

void Module::ReadVersion(MFileStream &file, ModuleVersion &target)
{
	target.major    = file.ReadInt32();
	target.minor    = file.ReadInt32();
	target.build    = file.ReadInt32();
	target.revision = file.ReadInt32();
}

// Some macros to help check the position of the file stream,
// for collections that contain a size. Note that this also
// includes an if statement that makes the code do nothing if
// the size is 0.
// (Semicolons intentionally left out to force a "missing ';'" error if you forget it.)
#define CHECKPOS_BEFORE() \
	uint32_t size = file.ReadUInt32(); /* The size of the rest of the data */ \
	if (size != 0) { \
		ios::pos_type posBefore = file.stream.tellg()

#define CHECKPOS_AFTER_(tbl) \
		ios::pos_type posAfter = file.stream.tellg(); \
		if (posBefore + (ios::pos_type)size != posAfter) \
			throw ModuleLoadException(file.fileName, "The actual size of the " #tbl " table did not match the expected size."); \
	}
#define CHECKPOS_AFTER(tbl) CHECKPOS_AFTER_(tbl)

void Module::ReadModuleRefs(MFileStream &file, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = file.ReadInt32();
	target->moduleRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		uint32_t id = file.ReadUInt32();
		if (id != ((i + 1) | IDMASK_MODULEREF))
			throw ModuleLoadException(file.fileName, "Invalid ModuleRef token ID.");
		// Module reference has a name followed by a minimum version
		String *modName = file.ReadString();
		ModuleVersion minVer;
		ReadVersion(file, minVer);

		Module *ref = OpenByName(modName);
		if (!ref->fullyOpened)
			throw ModuleLoadException(file.fileName, "Circular dependency detected.");
		if (ModuleVersion::Compare(ref->version, minVer) < 0)
			throw ModuleLoadException(file.fileName, "Dependent module has insufficient version.");

		target->moduleRefs.SetItem(i, ref);
	}

	CHECKPOS_AFTER(ModuleRef);
}

void Module::ReadTypeRefs(MFileStream &file, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = file.ReadInt32();
	target->typeRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		uint32_t id = file.ReadUInt32();
		if (id != ((i + 1) | IDMASK_TYPEREF))
			throw ModuleLoadException(file.fileName, "Invalid TypeRef token ID.");
		// Type reference has a name followed by a ModuleRef ID.
		String *typeName = file.ReadString();
		uint32_t modRef = file.ReadUInt32();

		const Module *owner = target->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(file.fileName, "Unresolved ModuleRef token in TypeRef.");

		const Type *type = owner->FindType(typeName, false);
		if (!type)
			throw ModuleLoadException(file.fileName, "Unresolved TypeRef.");

		target->typeRefs.SetItem(i, const_cast<Type*>(type));
	}

	CHECKPOS_AFTER(TypeRef);
}

void Module::ReadFunctionRefs(MFileStream &file, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = file.ReadInt32();
	target->functionRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		uint32_t id = file.ReadUInt32();
		if (id != ((i + 1) | IDMASK_FUNCTIONREF))
			throw ModuleLoadException(file.fileName, "Invalid FunctionRef token ID.");
		// Function reference has a name followed by a ModuleRef ID
		String *funcName = file.ReadString();
		uint32_t modRef = file.ReadUInt32();

		const Module *owner = target->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(file.fileName, "Invalid module token ID in FunctionRef.");

		Method *func = owner->FindGlobalFunction(funcName, false);
		if (!func)
			throw ModuleLoadException(file.fileName, "Unresolved FunctionRef.");

		target->functionRefs.SetItem(i, func);
	}

	CHECKPOS_AFTER(FunctionRef);
}

void Module::ReadFieldRefs(MFileStream &file, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = file.ReadInt32();
	target->fieldRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		uint32_t id = file.ReadUInt32();
		if (id != ((i + 1) | IDMASK_FIELDREF))
			throw ModuleLoadException(file.fileName, "Invalid FieldRef token ID.");
		// Field reference has a name followed by a TypeRef ID.
		String *fieldName = file.ReadString();
		uint32_t typeRef = file.ReadUInt32();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(file.fileName, "FieldRef must contain a TypeRef.");

		const Type *type = target->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(file.fileName, "Unresolved TypeRef token in FieldRef.");

		Member *member = type->GetMember(fieldName);
		if (!member)
			throw ModuleLoadException(file.fileName, "Unresolved FieldRef.");
		if (!(member->flags & M_FIELD))
			throw ModuleLoadException(file.fileName, "FieldRef does not refer to a field.");

		target->fieldRefs.SetItem(i, member);
	}

	CHECKPOS_AFTER(FieldRef);
}

void Module::ReadMethodRefs(MFileStream &file, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = file.ReadInt32();
	target->methodRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		uint32_t id = file.ReadUInt32();
		if (id != ((i + 1) | IDMASK_METHODREF))
			throw ModuleLoadException(file.fileName, "Invalid MethodRef token ID.");
		// Method reference has a name followed by a TypeRef ID.
		String *methodName = file.ReadString();
		uint32_t typeRef = file.ReadUInt32();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(file.fileName, "MethodRef must contain a TypeRef.");

		const Type *type = target->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(file.fileName, "Unresolved TypeRef token in MethodRef.");

		Member *member = type->GetMember(methodName);
		if (!member)
			throw ModuleLoadException(file.fileName, "Unresolved MethodRef.");
		if (!(member->flags & M_METHOD))
			throw ModuleLoadException(file.fileName, "MethodRef does not refer to a method.");

		target->methodRefs.SetItem(i, member->method);
	}

	CHECKPOS_AFTER(MethodRef);
}

void Module::ReadStringTable(MFileStream &file, Module *target)
{
	CHECKPOS_BEFORE();

	int32_t length = file.ReadInt32();
	target->strings.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		uint32_t id = file.ReadUInt32();
		if (id != ((i + 1) | IDMASK_STRING))
			throw ModuleLoadException(file.fileName, "Invalid String token ID.");

		String *value = file.ReadString();
		target->strings.SetItem(i, value);
	}

	CHECKPOS_AFTER(String);
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