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
	const int32_t length = ReadInt32();

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
	const int32_t length = ReadInt32();

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

char *ModuleReader::ReadCString()
{
	const int32_t length = ReadInt32();

	if (length == 0)
		return nullptr;

	char *output = new char[length];
	Read(output, length);

	return output;
}

Module::Module(ModuleMeta &meta) :
	// This initializer list is kind of silly
	name(meta.name), version(meta.version), fullyOpened(false),
	// defs
	functions(meta.functionCount),
	types(meta.typeCount),
	constants(meta.constantCount),
	fields(meta.fieldCount),
	methods(meta.methodCount),
	strings(0), // for now
	members(meta.functionCount + meta.typeCount + meta.constantCount),
	// refs
	moduleRefs(0),
	functionRefs(0),
	typeRefs(0),
	fieldRefs(0),
	methodRefs(0),
	methodStart(meta.methodStart)
{ }

Module::~Module()
{
	types.DeleteEntries();
	functions.DeleteEntries();
	fields.DeleteEntries();
	methods.DeleteEntries();

	strings.FreeEntries(); // these strings were allocated with malloc(), not new

	moduleRefs.DeleteEntries();
	typeRefs.DeleteEntries();
	functionRefs.DeleteEntries();
	fieldRefs.DeleteEntries();
	methodRefs.DeleteEntries();

	FreeNativeLibrary();
}

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

	if (token & IDMASK_STRING)
		return strings[TOKEN_INDEX(token)];

	return nullptr;
}


Module *Module::Find(String *name)
{
	using namespace std;

	for (vector<Module*>::iterator i = loadedModules.begin(); i != loadedModules.end(); ++i)
	{
		Module *mod = *i;
		if (String_Equals(mod->name, name))
			return mod;
	}

	return nullptr;
}


Module *Module::Open(const wchar_t *fileName)
{
	/*DWORD attrs = GetFileAttributesW(fileName);
	if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
		throw ModuleLoadException(fileName, "Module file does not exist.");*/

	Module *outputModule = nullptr;

	try
	{
		ModuleReader reader(fileName); // This SHOULD not fail. but it's C++, so who knows.
		VerifyMagicNumber(reader);

		reader.stream.seekg(module_file::dataStart, ios::beg);

		ModuleMeta meta;
		ReadModuleMeta(reader, meta);

		// ReadModuleMeta gives us just enough information to initialize the output module
		// and add it to the list of loaded modules.
		// It's not fully loaded yet, but we add it specifically so that we can detect
		// circular dependencies.
		Temp<Module> output(new Module(meta));
		loadedModules.push_back(output.GetValue());

		if (meta.nativeLib)
			output.GetValue()->LoadNativeLibrary(meta.nativeLib, fileName);

		// And these must be called in exactly this order!
		ReadModuleRefs(reader, output);   // moduleRefs
		ReadTypeRefs(reader, output);     // typeRefs
		ReadFunctionRefs(reader, output); // functionRefs
		ReadFieldRefs(reader, output);    // fieldRefs
		ReadMethodRefs(reader, output);   // methodRefs

		ReadStringTable(reader, output);  // strings

		ReadTypeDefs(reader, output);     // types
		ReadFunctionDefs(reader, output); // functions
		ReadConstantDefs(reader, output); // constants		

		TokenId mainMethodId = reader.ReadToken();
		if (mainMethodId != 0)
		{
			if ((mainMethodId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF &&
				(mainMethodId & IDMASK_MEMBERKIND) != IDMASK_FUNCTIONDEF)
				throw ModuleLoadException(reader.fileName, "Main method token ID must be a MethodDef or FunctionDef.");

			Method *mainMethod = output.GetValue()->FindMethod(mainMethodId);
			if (mainMethod == nullptr)
				throw ModuleLoadException(reader.fileName, "Unresolved main method token ID.");
			if (mainMethod->flags & M_INSTANCE)
				throw ModuleLoadException(reader.fileName, "Main method cannot be an instance method.");

			output.GetValue()->mainMethod = mainMethod;
		}

		outputModule = output.UseValue();
	}
	catch (std::ios_base::failure &ioError)
	{
		throw ModuleLoadException(fileName, ioError.what());
	}

	throw ModuleLoadException(fileName, "Function not fully implemented yet.");

	return outputModule;
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

	return output;
}

void Module::LoadNativeLibrary(String *nativeFileName, const wchar_t *path)
{
	// Native library files are ALWAYS loaded from the same folder
	// as the module file. Immer & mindig. 'path' contains the full
	// path and file name of the module file, so we strip the module
	// file name and append nativeFileName! Simple!
	size_t pathLen = wcslen(path);
	TempArr<wchar_t> pathBufTemp(new wchar_t[max(MAX_PATH, pathLen) + 1]);
	wchar_t *pathBuf = pathBufTemp.GetValue();
	pathBuf[pathLen] = L'\0';
	CopyMemoryT(pathBuf, path, pathLen);
	PathRemoveFileSpecW(pathBuf); // get the actual path!

	{
		const int fileNameWLen = String_ToWString(nullptr, nativeFileName);
		TempArr<wchar_t> fileNameW(new wchar_t[fileNameWLen]);
		String_ToWString(fileNameW.GetValue(), nativeFileName);

		PathAppendW(pathBuf, PathFindFileNameW(fileNameW.GetValue()));
	}

	// pathBuf should now contain a full path to the native module
	this->nativeLib = LoadLibraryW(pathBuf);

	// If this->nativeLib is null, then the library could not be loaded.
	if (!this->nativeLib)
		throw ModuleLoadException(path, "Could not load native library file.");
}

void Module::FreeNativeLibrary()
{
	if (nativeLib)
	{
		FreeLibrary(nativeLib);
		nativeLib = nullptr;
	}
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
	Temp<String, true> name(reader.ReadString()); // name
	ReadVersion(reader, target.version); // version

	// String map (skip)
	reader.SkipCollection();

	Temp<String, true> nativeLib(reader.ReadStringOrNull()); // nativeLib (nullptr if absent)

	target.typeCount     = reader.ReadInt32();  // typeCount
	target.functionCount = reader.ReadInt32();  // functionCount
	target.constantCount = reader.ReadInt32();  // constantCount
	target.fieldCount    = reader.ReadInt32();  // fieldCount
	target.methodCount   = reader.ReadInt32();  // methodCount
	target.methodStart   = reader.ReadUInt32(); // methodStart

	target.name = name.UseValue();
	target.nativeLib = nativeLib.UseValue();
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
	const uint32_t size = reader.ReadUInt32(); /* The size of the rest of the data */ \
	if (size != 0) { \
		const ios::pos_type posBefore = reader.stream.tellg()

#define CHECKPOS_AFTER_(tbl) \
		const ios::pos_type posAfter = reader.stream.tellg(); \
		if (posBefore + (ios::pos_type)size != posAfter) \
			throw ModuleLoadException(reader.fileName, "The actual size of the " #tbl " table did not match the expected size."); \
	}
#define CHECKPOS_AFTER(tbl) CHECKPOS_AFTER_(tbl)

void Module::ReadModuleRefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->moduleRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->moduleRefs.GetNextId(IDMASK_MODULEREF))
			throw ModuleLoadException(reader.fileName, "Invalid ModuleRef token ID.");
		// Module reference has a name followed by a minimum version
		Temp<String, true> modName(reader.ReadString());
		ModuleVersion minVer;
		ReadVersion(reader, minVer);

		Module *ref = OpenByName(modName.GetValue());
		if (!ref->fullyOpened)
			throw ModuleLoadException(reader.fileName, "Circular dependency detected.");
		if (ModuleVersion::Compare(ref->version, minVer) < 0)
			throw ModuleLoadException(reader.fileName, "Dependent module has insufficient version.");

		module->moduleRefs.Add(ref);
	}

	CHECKPOS_AFTER(ModuleRef);
}

void Module::ReadTypeRefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->typeRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->typeRefs.GetNextId(IDMASK_TYPEREF))
			throw ModuleLoadException(reader.fileName, "Invalid TypeRef token ID.");
		// Type reference has a name followed by a ModuleRef ID.
		Temp<String, true> typeName(reader.ReadString());
		TokenId modRef = reader.ReadToken();

		const Module *owner = module->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.fileName, "Unresolved ModuleRef token in TypeRef.");

		const Type *type = owner->FindType(typeName.GetValue(), false);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef.");

		module->typeRefs.Add(const_cast<Type*>(type));
	}

	CHECKPOS_AFTER(TypeRef);
}

void Module::ReadFunctionRefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->functionRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->functionRefs.GetNextId(IDMASK_FUNCTIONREF))
			throw ModuleLoadException(reader.fileName, "Invalid FunctionRef token ID.");
		// Function reference has a name followed by a ModuleRef ID
		Temp<String, true> funcName(reader.ReadString());
		TokenId modRef = reader.ReadToken();

		const Module *owner = module->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.fileName, "Invalid module token ID in FunctionRef.");

		Method *func = owner->FindGlobalFunction(funcName.GetValue(), false);
		if (!func)
			throw ModuleLoadException(reader.fileName, "Unresolved FunctionRef.");

		module->functionRefs.Add(func);
	}

	CHECKPOS_AFTER(FunctionRef);
}

void Module::ReadFieldRefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->fieldRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->fieldRefs.GetNextId(IDMASK_FIELDREF))
			throw ModuleLoadException(reader.fileName, "Invalid FieldRef token ID.");
		// Field reference has a name followed by a TypeRef ID.
		Temp<String, true> fieldName(reader.ReadString());
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.fileName, "FieldRef must contain a TypeRef.");

		const Type *type = module->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef token in FieldRef.");

		Member *member = type->GetMember(fieldName.GetValue());
		if (!member)
			throw ModuleLoadException(reader.fileName, "Unresolved FieldRef.");
		if (!(member->flags & M_FIELD))
			throw ModuleLoadException(reader.fileName, "FieldRef does not refer to a field.");

		module->fieldRefs.Add((Field*)member);
	}

	CHECKPOS_AFTER(FieldRef);
}

void Module::ReadMethodRefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->methodRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->methodRefs.GetNextId(IDMASK_METHODREF))
			throw ModuleLoadException(reader.fileName, "Invalid MethodRef token ID.");
		// Method reference has a name followed by a TypeRef ID.
		Temp<String, true> methodName(reader.ReadString());
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.fileName, "MethodRef must contain a TypeRef.");

		const Type *type = module->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef token in MethodRef.");

		Member *member = type->GetMember(methodName.GetValue());
		if (!member)
			throw ModuleLoadException(reader.fileName, "Unresolved MethodRef.");
		if (!(member->flags & M_METHOD))
			throw ModuleLoadException(reader.fileName, "MethodRef does not refer to a method.");

		module->methodRefs.Add((Method*)member);
	}

	CHECKPOS_AFTER(MethodRef);
}

void Module::ReadStringTable(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->strings.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->strings.GetNextId(IDMASK_STRING))
			throw ModuleLoadException(reader.fileName, "Invalid String token ID.");

		Temp<String, true> value(reader.ReadString());
		module->strings.Add(value.UseValue());
	}

	CHECKPOS_AFTER(String);
}

void Module::ReadTypeDefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->types.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->types.GetNextId(IDMASK_TYPEDEF))
			throw ModuleLoadException(reader.fileName, "Invalid TypeDef token ID.");

		Type *type = ReadSingleType(reader, target, id);
		module->types.Add(type);
		module->members.Add(type->fullName, ModuleMember(type, (type->flags & TYPE_PRIVATE) == TYPE_PRIVATE));
	}

	CHECKPOS_AFTER(TypeDef);
}

void Module::ReadFunctionDefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->functions.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->functions.GetNextId(IDMASK_FUNCTIONDEF))
			throw ModuleLoadException(reader.fileName, "Invalid FunctionDef token ID.");

		Temp<Method> tempFunction(ReadSingleMethod(reader, target));
		Method *function = tempFunction.GetValue();

		if (!module->members.Add(function->name, ModuleMember(function, (function->flags & M_PRIVATE) == M_PRIVATE)))
			throw ModuleLoadException(reader.fileName, "Duplicate global member name.");
		module->functions.Add(function);

		tempFunction.UseValue();
	}

	CHECKPOS_AFTER(FunctionDef);
}

void Module::ReadConstantDefs(ModuleReader &reader, Temp<Module> &target)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	Module *module = target.GetValue();
	module->constants.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->constants.GetNextId(IDMASK_CONSTANTDEF))
			throw ModuleLoadException(reader.fileName, "Invalid ConstantDef token ID.");

		enum ConstantFlags { CONST_PUBLIC = 0x01, CONST_PRIVATE = 0x02 };
		ConstantFlags flags = (ConstantFlags)reader.ReadUInt32();

		Temp<String, true> name(reader.ReadString());
		TokenId typeId = reader.ReadToken();

		const Type *type = module->FindType(typeId);
		if (type == nullptr)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef or TypeDef token ID in ConstantDef.");
		if (type != stdTypes.String && !(type->flags & TYPE_PRIMITIVE))
			throw ModuleLoadException(reader.fileName, "ConstantDef type must be primitive or aves.String.");

		int64_t value = reader.ReadInt64();

		Value constant;
		constant.type = type;

		if (type == stdTypes.String)
		{
			String *str = module->FindString((TokenId)value);
			if (str == nullptr)
				throw ModuleLoadException(reader.fileName, "Unresolved String token ID in ConstantDef.");
			constant.common.string = str;
		}
		else
			constant.integer = value;

		module->constants.Add(constant);
		module->members.Add(name.UseValue(), ModuleMember(constant, (flags & CONST_PRIVATE) == CONST_PRIVATE));
	}

	CHECKPOS_AFTER(ConstantDef);
}

Type *Module::ReadSingleType(ModuleReader &reader, Temp<Module> &target, const TokenId typeId)
{
	TypeFlags flags = (TypeFlags)reader.ReadUInt32();
	Temp<String, true> name(reader.ReadString());

	TokenId baseTypeId   = reader.ReadToken();
	TokenId sharedTypeId = reader.ReadToken();

	const Type *baseType = nullptr;
	if (baseTypeId != 0)
	{
		if (baseTypeId == typeId)
			throw ModuleLoadException(reader.fileName, "A type cannot have itself as its base type.");
		baseType = target.GetValue()->FindType(baseTypeId);
		if (baseType == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve base type ID.");
	}

	const Type *sharedType = nullptr;
	if (sharedTypeId != 0)
	{
		if ((sharedTypeId & IDMASK_MEMBERKIND) != IDMASK_TYPEDEF)
			throw ModuleLoadException(reader.fileName, "A shared type must be a TypeDef.");
		if (sharedTypeId == typeId)
			throw ModuleLoadException(reader.fileName, "A type cannot have itself as its shared type.");
		sharedType = target.GetValue()->FindType(sharedTypeId);
		if (sharedType == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve shared type ID.");
	}

	const int32_t memberCount = reader.ReadInt32(); // memberCount
	Temp<Type> output(new Type(memberCount));
	Type *type = output.GetValue();
	type->flags        = flags;
	type->baseType     = baseType;
	type->sharedType   = sharedType;
	type->fieldsOffset = baseType ? baseType->fieldsOffset + baseType->size : 0;
	type->fullName     = name.UseValue();
	type->module       = target.GetValue();

	ReadFields(reader, target, output);     // fields
	ReadMethods(reader, target, output);    // methods
	ReadProperties(reader, target, output); // properties
	ReadOperators(reader, target, output);  // operators

	TempArr<char> initer(reader.ReadCString());
	if (initer.GetValue() != nullptr)
	{
		// Find the entry point, whoo
		TypeInitializer func = (TypeInitializer)GetProcAddress(target.GetValue()->nativeLib, initer.GetValue());
		if (func == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not locate type initializer entry point.");
		func(output.GetValue());
	}

	output.UseValue();
	TryRegisterStandardType(type);
	return type;
}

void Module::ReadFields(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = targetModule.GetValue();
	Type *type = targetType.GetValue();

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->fields.GetNextId(IDMASK_FIELDDEF))
			throw ModuleLoadException(reader.fileName, "Invalid FieldDef token ID.");

		enum FieldFlags
		{
			FIELD_PUBLIC    = 0x01,
			FIELD_PRIVATE   = 0x02,
			FIELD_PROTECTED = 0x04,
			FIELD_INSTANCE  = 0x08,
			FIELD_HASVALUE  = 0x10,
		};
		FieldFlags fieldFlags = (FieldFlags)reader.ReadInt32();
		if ((fieldFlags & FIELD_HASVALUE) && (fieldFlags & FIELD_INSTANCE))
			throw ModuleLoadException(reader.fileName, "The field flags hasValue and instance cannot be used together.");
		MemberFlags flags = (MemberFlags)0;

		if (fieldFlags & FIELD_PUBLIC)
			flags = flags | M_PUBLIC;
		else if (fieldFlags & FIELD_PRIVATE)
			flags = flags | M_PRIVATE;
		else if (fieldFlags & FIELD_PROTECTED)
			flags = flags | M_PROTECTED;

		if (fieldFlags & FIELD_INSTANCE)
			flags = flags | M_INSTANCE;

		Temp<String, true> name(reader.ReadString());

		// Skip constant value, if there is one
		if (fieldFlags & FIELD_HASVALUE)
			reader.stream.seekg(sizeof(TokenId) + sizeof(uint64_t), ios::cur);

		Temp<Field> tempField(new Field(name.UseValue(), type, flags));
		Field *field = tempField.GetValue();

		if (!type->members.Add(field->name, field))
			throw ModuleLoadException(reader.fileName, "Duplicate member name in type.");
		module->fields.Add(field);

		if (!field->IsStatic())
		{
			field->offset = type->fieldsOffset + type->size;
			type->fieldCount++;
			type->size += sizeof(Value);
		}
		// If the field is static, we do not allocate any storage for it until
		// the parent type's static constructor is about to be run, as that will
		// be the first time the field is referred to.

		tempField.UseValue();
	}

	CHECKPOS_AFTER(FieldDef);
}

void Module::ReadMethods(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = targetModule.GetValue();
	Type *type = targetType.GetValue();

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->methods.GetNextId(IDMASK_METHODDEF))
			throw ModuleLoadException(reader.fileName, "Invalid MethodDef token ID.");

		Temp<Method> tempMethod(ReadSingleMethod(reader, targetModule));
		Method *method = tempMethod.GetValue();

		if (!type->members.Add(method->name, method))
			throw ModuleLoadException(reader.fileName, "Duplicate member name in type.");
		module->methods.Add(method);
		method->declType = type;

		tempMethod.UseValue();
	}

	CHECKPOS_AFTER(MethodDef);
}

void Module::ReadProperties(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = targetModule.GetValue();
	Type *type = targetType.GetValue();

	for (int32_t i = 0; i < length; i++)
	{
		Temp<String> name(reader.ReadString());
		TokenId getterId = reader.ReadToken();
		TokenId setterId = reader.ReadToken();

		MemberFlags flags = (MemberFlags)0;
		Method *getter = nullptr;
		if (getterId != 0)
		{
			if ((getterId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF)
				throw ModuleLoadException(reader.fileName, "Property getter must be a MethodDef.");
			getter = module->FindMethod(getterId);
			if (!getter)
				throw ModuleLoadException(reader.fileName, "Unresolved MethodDef token ID in property getter.");
			if (getter->declType != type)
				throw ModuleLoadException(reader.fileName, "Property getter must refer to a method in the same type as the property.");

			flags = getter->flags & ~M_IMPL;
		}

		Method *setter = nullptr;
		if (setterId != 0)
		{
			if ((setterId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF)
				throw ModuleLoadException(reader.fileName, "Property setter must be a MethodDef.");
			setter = module->FindMethod(setterId);
			if (!setter)
				throw ModuleLoadException(reader.fileName, "Unresolved MethodDef token ID in property setter.");
			if (setter->declType != type)
				throw ModuleLoadException(reader.fileName, "Property setter must refer to a method in the same type as the property.");

			MemberFlags setterFlags = setter->flags & ~M_IMPL;
			if (flags && setterFlags != flags)
				throw ModuleLoadException(reader.fileName, "Property getter and setter must have the same accessibility, and matching abstract, virtual, sealed and instance flags.");

			// We've just determined that either the flags the same, or 'flags' is empty,
			// so overwriting here is fine.
			flags = setterFlags;
		}

		if (!getter && !setter)
			throw ModuleLoadException(reader.fileName, "Property must have at least one accessor.");

		Temp<Property> tempProp(new Property(name.UseValue(), type, flags));
		Property *prop = tempProp.GetValue();
		prop->getter = getter;
		prop->setter = setter;

		if (!type->members.Add(prop->name, prop))
			throw ModuleLoadException(reader.fileName, "Duplicate member name in type.");

		tempProp.UseValue();
	}

	CHECKPOS_AFTER(PropertyDef);
}

void Module::ReadOperators(ModuleReader &reader, Temp<Module> &targetModule, Temp<Type> &targetType)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = targetModule.GetValue();
	Type *type = targetType.GetValue();

	for (int32_t i = 0; i < length; i++)
	{
		Operator op = (Operator)reader.ReadUInt8();
		TokenId methodId = reader.ReadToken();

		if ((methodId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF)
			throw ModuleLoadException(reader.fileName, "Operator method must be a MethodDef.");
		Method *method = module->FindMethod(methodId);
		if (!method)
			throw ModuleLoadException(reader.fileName, "Unresolved MethodDef token ID in operator.");
		if (method->declType != type)
			throw ModuleLoadException(reader.fileName, "Operator method must be in the same type as the property.");
		if (type->operators[op] != nullptr)
			throw ModuleLoadException(reader.fileName, "Duplicate operator declaration.");

		type->operators[op] = method;
	}

	CHECKPOS_AFTER(OperatorDef);
}

Method *Module::ReadSingleMethod(ModuleReader &reader, Temp<Module> &target)
{
	FileMethodFlags methodFlags = (FileMethodFlags)reader.ReadUInt32();

	Temp<String, true> name(reader.ReadString());

	const uint32_t size = reader.ReadUInt32();
	if (size == 0)
		throw ModuleLoadException(reader.fileName, "Method found without overloads.");
	
	const ios::pos_type posBefore = reader.stream.tellg();
	const int32_t overloadCount = reader.ReadInt32();

	if (overloadCount == 0)
		throw ModuleLoadException(reader.fileName, "Method found without overloads.");

	MemberFlags memberFlags = M_NONE;
	if (methodFlags & FM_PUBLIC)
		memberFlags = memberFlags | M_PUBLIC;
	else if (methodFlags & FM_PRIVATE)
		memberFlags = memberFlags | M_PRIVATE;
	else if (methodFlags & FM_PROTECTED)
		memberFlags = memberFlags | M_PROTECTED;
	if (methodFlags & FM_INSTANCE)
		memberFlags = memberFlags | M_INSTANCE;
	if (methodFlags & FM_IMPL)
		memberFlags = memberFlags | M_IMPL;

	Temp<Method> output(new Method(name.UseValue(), target.GetValue(), memberFlags));
	Method *method = output.GetValue();

	TempArr<Method::Overload> tempOverloads(new Method::Overload[overloadCount]);
	Method::Overload *overloads = tempOverloads.GetValue();

	for (int32_t i = 0; i < overloadCount; i++)
	{
		OverloadFlags flags = (OverloadFlags)reader.ReadUInt32();

		Method::Overload *ov = overloads + i;
		ov->group = method;

		// Parameter count
		uint16_t paramCount = reader.ReadUInt16();
		// Skip parameter names (not needed)
		for (int32_t k = 0; k < paramCount; k++)
		{
			int32_t nameLength = reader.ReadInt32();
			reader.stream.seekg(nameLength * sizeof(uchar), ios::cur);
		}
		ov->paramCount = paramCount;

		// Flags
		ov->flags = (MethodFlags)0;
		if (methodFlags & FM_CTOR)
			ov->flags = ov->flags | METHOD_CTOR;
		if (methodFlags & FM_INSTANCE)
			ov->flags = ov->flags | METHOD_INSTANCE;
		if (flags & OV_VAREND)
			ov->flags = ov->flags | METHOD_VAR_END;
		if (flags & OV_VARSTART)
			ov->flags = ov->flags | METHOD_VAR_START;
		if (flags & OV_VIRTUAL)
			ov->flags = ov->flags | METHOD_VIRTUAL;
		if (flags & OV_ABSTRACT)
			ov->flags = ov->flags | METHOD_ABSTRACT;

		// Header
		int32_t tryCount = 0;
		TempArr<Method::TryBlock> tries(nullptr);
		if (flags & OV_SHORTHEADER)
		{
			ov->optionalParamCount = ov->locals = 0;
			ov->maxStack = 8;
		}
		else
		{
			ov->optionalParamCount = reader.ReadUInt16();
			ov->locals = reader.ReadUInt16();
			ov->maxStack = reader.ReadUInt16();
			tries = TempArr<Method::TryBlock>(ReadTryBlocks(reader, target, tryCount));
		}

		ov->tryBlockCount = tryCount;
		ov->tryBlocks = tries.UseValue();

		// Body
		if (!(flags & OV_ABSTRACT))
		{
			if (flags & OV_NATIVE)
			{
				TempArr<char> entryPointName(reader.ReadCString());
				NativeMethod entryPoint = (NativeMethod)GetProcAddress(target.GetValue()->nativeLib, entryPointName.GetValue());
				if (entryPoint == nullptr)
					throw ModuleLoadException(reader.fileName, "Could not locate entry point of native method.");
				ov->nativeEntry = entryPoint;
			}
			else
			{
				uint32_t offset = reader.ReadUInt32(); // The offset of the first instruction in the method, relative to the method block
				uint32_t length = reader.ReadUInt32(); // The length of the body, in bytes

				const ios::pos_type posCurrent = reader.stream.tellg(); // Resumption point

				// Read the method body
				reader.stream.seekg(target.GetValue()->methodStart, ios::beg);
				TempArr<uint8_t> body(new uint8_t[length]);
				reader.Read(body.GetValue(), length);

				// Return to previous position
				reader.stream.seekg(posCurrent, ios::beg);

				ov->length = length;
				ov->entry = body.UseValue();
			}
		}
	}

	const ios::pos_type posAfter = reader.stream.tellg();
	if (posBefore + (ios::pos_type)size != posAfter)
			throw ModuleLoadException(reader.fileName, "The actual size of the overloads table did not match the expected size.");

	method->overloadCount = overloadCount;
	method->overloads = tempOverloads.UseValue();

	return output.UseValue();
}

Method::TryBlock *Module::ReadTryBlocks(ModuleReader &reader, Temp<Module> &targetModule, int32_t &tryCount)
{
	TempArr<Method::TryBlock> output(nullptr);

	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	Module *module = targetModule.GetValue();

	output = TempArr<Method::TryBlock>(new Method::TryBlock[length]);
	Method::TryBlock *tries = output.GetValue();

	for (int32_t i = 0; i < length; i++)
	{
		Method::TryBlock::TryKind kind = (Method::TryBlock::TryKind)reader.ReadUInt8();
		uint32_t tryStart = reader.ReadUInt32();
		uint32_t tryEnd   = reader.ReadUInt32();

		Method::TryBlock *curTry = tries + i;
		*curTry = Method::TryBlock(kind, tryStart, tryEnd);

		if (kind == Method::TryBlock::FINALLY)
		{
			curTry->finallyBlock.finallyStart = reader.ReadUInt32();
			curTry->finallyBlock.finallyEnd = reader.ReadUInt32();
		}
		else if (kind == Method::TryBlock::CATCH)
		{
			uint32_t catchSize = reader.ReadUInt32();
			if (catchSize != 0)
			{
				int32_t catchLength = reader.ReadInt32();
				TempArr<Method::CatchBlock> catches(new Method::CatchBlock[catchLength]);

				for (int32_t i = 0; i < catchLength; i++)
				{
					Method::CatchBlock *curCatch = catches.GetValue() + i;

					curCatch->caughtTypeId = reader.ReadToken();
					// Try to resolve the type right away. If it fails, do it when the method
					// is initialized instead.
					if (module->FindType(curCatch->caughtTypeId))
						curCatch->caughtType = module->FindType(curCatch->caughtTypeId);

					curCatch->catchStart   = reader.ReadUInt32();
					curCatch->catchEnd     = reader.ReadUInt32();
				}

				curTry->catches.count = catchLength;
				curTry->catches.blocks = catches.UseValue();
			}
		}
	}

	tryCount = length;

	CHECKPOS_AFTER(tries);

	return output.UseValue();
}

void Module::TryRegisterStandardType(Type *type)
{
	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType stdType = std_type_names::Types[i];
		if (String_Equals(type->fullName, stdType.name))
		{
			if (stdTypes.*(stdType.member) == nullptr)
				stdTypes.*(stdType.member) = type;
			break;
		}
	}
}


// Paper thin API wrapper functions, whoo!

OVUM_API ModuleHandle FindModule(String *name)
{
	return Module::Find(name);
}

OVUM_API TypeHandle Module_FindType(ModuleHandle module, String *name, bool includeInternal)
{
	return module->FindType(name, includeInternal);
}

OVUM_API MethodHandle Module_FindGlobalFunction(ModuleHandle module, String *name, bool includeInternal)
{
	return module->FindGlobalFunction(name, includeInternal);
}

OVUM_API const bool Module_FindConstant(ModuleHandle module, String *name, bool includeInternal, Value &result)
{
	return module->FindConstant(name, includeInternal, result);
}