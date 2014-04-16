#include "ov_module.internal.h"
#include "modulereader.internal.h"
#include "ov_stringbuffer.internal.h"
#include "ov_debug_symbols.internal.h"
#include "refsignature.internal.h"
#include <memory>
#include <shlwapi.h>

using namespace std;

Module::Pool *Module::loadedModules = nullptr;
const char *const Module::NativeModuleIniterName = "OvumModuleMain";

namespace module_file
{
	// The magic number that must be present in all Ovum modules.
	const char MagicNumber[] = { 'O', 'V', 'M', 'M' };

	// The start of the "real" data in the module.
	const unsigned int DataStart = 16;

	// The minimum supported file format version
	const uint32_t MinFileFormatVersion = 0x00000100u;

	// The maximum supported file format version
	const uint32_t MaxFileFormatVersion = 0x00000100u;
}


Module::Module(uint32_t fileFormatVersion, ModuleMeta &meta) :
	// This initializer list is kind of silly
	fileFormatVersion(fileFormatVersion),
	name(meta.name), version(meta.version), fullyOpened(false),
	// defs
	functions(meta.functionCount),
	types(meta.typeCount),
	constants(meta.constantCount),
	fields(meta.fieldCount),
	methods(meta.methodCount),
	strings(0), // for now, initialized with stuff later
	members(meta.functionCount + meta.typeCount + meta.constantCount),
	// refs
	moduleRefs(0),
	functionRefs(0),
	typeRefs(0),
	fieldRefs(0),
	methodRefs(0),
	methodStart(meta.methodStart),
	nativeLib(nullptr),
	mainMethod(nullptr),
	debugData(nullptr)
{ }

Module::~Module()
{
#ifdef PRINT_DEBUG_INFO
	std::wcout << L"Releasing module: ";
	VM::PrintLn(this->name);
#endif
	// Note: Don't touch any of the string values.
	// They're managed by the GC, so we let her clean it up.

	functions.DeleteEntries();
	fields.DeleteEntries();
	methods.DeleteEntries();
	types.DeleteEntries();

	//members.DeleteValues(); // Nope, these values are not pointers

	// Don't delete the refs here! They are in their own modules.

	FreeNativeLibrary();

	if (debugData != nullptr)
		delete debugData;
}

Type *Module::FindType(String *name, bool includeInternal) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return nullptr;

	if (!includeInternal && (member.flags & ModuleMemberFlags::PROTECTION) == ModuleMemberFlags::INTERNAL
		||
		(member.flags & ModuleMemberFlags::KIND) != ModuleMemberFlags::TYPE)
		return nullptr;

	return member.type;
}

Method *Module::FindGlobalFunction(String *name, bool includeInternal) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return nullptr;

	if (!includeInternal && (member.flags & ModuleMemberFlags::PROTECTION) == ModuleMemberFlags::INTERNAL
		||
		(member.flags & ModuleMemberFlags::KIND) != ModuleMemberFlags::FUNCTION)
		return nullptr;

	return member.function;
}

bool Module::FindConstant(String *name, bool includeInternal, Value &result) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return false;

	if (!includeInternal && (member.flags & ModuleMemberFlags::PROTECTION) == ModuleMemberFlags::INTERNAL
		||
		(member.flags & ModuleMemberFlags::KIND) != ModuleMemberFlags::CONSTANT)
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

Type *Module::FindType(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_TYPEDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_TYPEREF);

	if ((token & IDMASK_MEMBERKIND) == IDMASK_TYPEDEF)
		return types[TOKEN_INDEX(token)];
	else if ((token & IDMASK_MEMBERKIND) == IDMASK_TYPEREF)
		return typeRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

Method *Module::FindMethod(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_METHODDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_METHODREF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_FUNCTIONDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_FUNCTIONREF);

	uint32_t idx = TOKEN_INDEX(token);

	if ((token & IDMASK_MEMBERKIND) == IDMASK_METHODDEF)
		return methods[idx];
	else if ((token & IDMASK_MEMBERKIND) == IDMASK_METHODREF)
		return methodRefs[idx];
	else if ((token & IDMASK_MEMBERKIND) == IDMASK_FUNCTIONDEF)
		return functions[idx];
	else if ((token & IDMASK_MEMBERKIND) == IDMASK_FUNCTIONREF)
		return functionRefs[idx];

	return nullptr; // not found
}

Field *Module::FindField(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_FIELDDEF ||
		(token & IDMASK_MEMBERKIND) == IDMASK_FIELDREF);

	if ((token & IDMASK_MEMBERKIND) == IDMASK_FIELDDEF)
		return fields[TOKEN_INDEX(token)];
	else if ((token & IDMASK_MEMBERKIND) == IDMASK_FIELDREF)
		return fieldRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

String *Module::FindString(TokenId token) const
{
	assert((token & IDMASK_MEMBERKIND) == IDMASK_STRING);

	if ((token & IDMASK_MEMBERKIND) == IDMASK_STRING)
		return strings[TOKEN_INDEX(token)];

	return nullptr;
}

Method *Module::GetMainMethod() const
{
	return mainMethod;
}


void *Module::FindNativeFunction(const char *name)
{
	if (nativeLib != nullptr)
		return FindNativeEntryPoint(name);
	return nullptr;
}


Module *Module::Find(String *name)
{
	using namespace std;

	return loadedModules->Get(name);
}


Module *Module::Open(const wchar_t *fileName)
{
	Module *outputModule = nullptr;

	try
	{
		ModuleReader reader; // This SHOULD not fail. but it's C++, so who knows.
		reader.Open(fileName);
		VerifyMagicNumber(reader);
		uint32_t fileFormatVersion = reader.ReadUInt32();
		if (fileFormatVersion < module_file::MinFileFormatVersion ||
			fileFormatVersion > module_file::MaxFileFormatVersion)
			throw ModuleLoadException(fileName, "Unsupported module file format version.");

		reader.Seek(module_file::DataStart, SeekOrigin::BEGIN);

		ModuleMeta meta;
		ReadModuleMeta(reader, meta);

		// ReadModuleMeta gives us just enough information to initialize the output module
		// and add it to the list of loaded modules.
		// It's not fully loaded yet, but we add it specifically so that we can detect
		// circular dependencies.
		unique_ptr<Module> output(new Module(fileFormatVersion, meta));
		loadedModules->Add(output.get());

		if (meta.nativeLib)
			output->LoadNativeLibrary(meta.nativeLib, fileName);

		ReadStringTable(reader, output.get());  // strings

		// And these must be called in exactly this order!
		ReadModuleRefs(reader, output.get());   // moduleRefs
		ReadTypeRefs(reader, output.get());     // typeRefs
		ReadFunctionRefs(reader, output.get()); // functionRefs
		ReadFieldRefs(reader, output.get());    // fieldRefs
		ReadMethodRefs(reader, output.get());   // methodRefs

		ReadTypeDefs(reader, output.get());     // types
		ReadFunctionDefs(reader, output.get()); // functions
		ReadConstantDefs(reader, output.get()); // constants

		TokenId mainMethodId = reader.ReadToken();
		if (mainMethodId != 0)
		{
			if ((mainMethodId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF &&
				(mainMethodId & IDMASK_MEMBERKIND) != IDMASK_FUNCTIONDEF)
				throw ModuleLoadException(reader.fileName, "Main method token ID must be a MethodDef or FunctionDef.");

			Method *mainMethod = output->FindMethod(mainMethodId);
			if (mainMethod == nullptr)
				throw ModuleLoadException(reader.fileName, "Unresolved main method token ID.");
			if ((mainMethod->flags & MemberFlags::INSTANCE) != MemberFlags::NONE)
				throw ModuleLoadException(reader.fileName, "Main method cannot be an instance method.");

			output->mainMethod = mainMethod;
		}

		if (output->nativeLib)
		{
			NativeModuleMain nativeMain = (NativeModuleMain)output->FindNativeEntryPoint(Module::NativeModuleIniterName);
			if (nativeMain)
				nativeMain(output.get());
		}

		debug::ModuleDebugData::TryLoad(fileName, output.get());

		outputModule = output.release();
	}
	catch (ModuleIOException &ioError)
	{
		throw ModuleLoadException(wstring(fileName), ioError.what());
	}
	catch (std::bad_alloc&)
	{
		throw ModuleLoadException(wstring(fileName), "Out of memory");
	}

	outputModule->fullyOpened = true;

	return outputModule;
}

Module *Module::OpenByName(String *name)
{
	Module *mod;
	if (mod = Find(name))
		return mod;
	
	StringBuffer moduleFileName(max(VM::vm->startupPath->length, VM::vm->modulePath->length) + name->length + 16);

	const int pathCount = 2;
	String *paths[pathCount] = { VM::vm->startupPath, VM::vm->modulePath };

	unique_ptr<wchar_t[]> filePath;

	for (int i = 0; i < pathCount; i++)
	{
		moduleFileName.Clear();

		moduleFileName.Append(paths[i]);
		if (!moduleFileName.EndsWith('\\'))
			moduleFileName.Append('\\');
		moduleFileName.Append(name);
		moduleFileName.Append(4, ".ovm");

		const int filePathLength = moduleFileName.ToWString(nullptr);
		filePath = unique_ptr<wchar_t[]>(new wchar_t[filePathLength]);
		moduleFileName.ToWString(filePath.get());

		DWORD attrs = GetFileAttributesW(filePath.get());
		if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
			break; // we've found our file! \o/

		filePath.reset();
	}

	if (filePath.get() == nullptr) // not found
	{
		const int moduleNameLength = String_ToWString(nullptr, name);
		unique_ptr<wchar_t[]> wname(new wchar_t[moduleNameLength]);
		String_ToWString(wname.get(), name);
		throw ModuleLoadException(wstring(wname.get()), "Could not locate the module file.");
	}

	if (VM::vm->verbose)
	{
		VM::Printf(L"Loading module '%ls' ", name);
		wprintf(L"from file '%ls'\n", filePath.get());
	}

	Module *output = Open(filePath.get());

	if (VM::vm->verbose)
		VM::Printf(L"Successfully loaded module '%ls'\n", name);

	return output;
}

void Module::Init()
{
	if (loadedModules == nullptr)
		loadedModules = new Pool();
}

void Module::Unload()
{
	using namespace std;

	delete loadedModules;
}

void Module::LoadNativeLibrary(String *nativeFileName, const wchar_t *path)
{
	// Native library files are ALWAYS loaded from the same folder
	// as the module file. Immer & mindig. 'path' contains the full
	// path and file name of the module file, so we strip the module
	// file name and append nativeFileName! Simple!
	size_t pathLen = wcslen(path);
	unique_ptr<wchar_t[]> pathBufTemp(new wchar_t[max(MAX_PATH, pathLen) + 1]);
	wchar_t *pathBuf = pathBufTemp.get();
	pathBuf[pathLen] = L'\0';
	CopyMemoryT(pathBuf, path, pathLen);
	PathRemoveFileSpecW(pathBuf); // get the actual path!

	{
		const int fileNameWLen = String_ToWString(nullptr, nativeFileName);
		unique_ptr<wchar_t[]> fileNameW(new wchar_t[fileNameWLen]);
		String_ToWString(fileNameW.get(), nativeFileName);

		PathAppendW(pathBuf, PathFindFileNameW(fileNameW.get()));
	}

	// pathBuf should now contain a full path to the native module
	this->nativeLib = LoadLibraryW(pathBuf);

	// If this->nativeLib is null, then the library could not be loaded.
	if (!this->nativeLib)
		throw ModuleLoadException(wstring(path), "Could not load native library file.");
}

void *Module::FindNativeEntryPoint(const char *name)
{
	return GetProcAddress(this->nativeLib, name);
}

void Module::FreeNativeLibrary()
{
	if (nativeLib)
	{
		FreeLibrary(this->nativeLib);
		this->nativeLib = nullptr;
	}
}

void Module::VerifyMagicNumber(ModuleReader &reader)
{
	char magicNumber[4];
	reader.Read(magicNumber, 4);
	for (int i = 0; i < 4; i++)
		if (magicNumber[i] != module_file::MagicNumber[i])
			throw ModuleLoadException(reader.fileName, "Invalid magic number in file.");
}

void Module::ReadModuleMeta(ModuleReader &reader, ModuleMeta &target)
{
	target.name = reader.ReadString();
	ReadVersion(reader, target.version);

	// String map (skip)
	reader.SkipCollection();

	target.nativeLib = reader.ReadStringOrNull(); // null if absent

	target.typeCount     = reader.ReadInt32();
	target.functionCount = reader.ReadInt32();
	target.constantCount = reader.ReadInt32();
	target.fieldCount    = reader.ReadInt32();
	target.methodCount   = reader.ReadInt32();
	target.methodStart   = reader.ReadUInt32() + sizeof(uint32_t); // methodStart + method block size prefix
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
		const long posBefore = reader.GetPosition()

#define CHECKPOS_AFTER_(tbl) \
		const long posAfter = reader.GetPosition(); \
		if (posBefore + (long)size != posAfter) \
			throw ModuleLoadException(reader.fileName, "The actual size of the " #tbl " table did not match the expected size."); \
	}
#define CHECKPOS_AFTER(tbl) CHECKPOS_AFTER_(tbl)

void Module::ReadModuleRefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	module->moduleRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->moduleRefs.GetNextId(IDMASK_MODULEREF))
			throw ModuleLoadException(reader.fileName, "Invalid ModuleRef token ID.");
		// Module reference has a name followed by a minimum version
		String *modName = module->FindString(reader.ReadToken());
		if (modName == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID for ModuleRef name.");
		ModuleVersion minVer;
		ReadVersion(reader, minVer);

		Module *ref = OpenByName(modName);
		if (!ref->fullyOpened)
			throw ModuleLoadException(reader.fileName, "Circular dependency detected.");
		if (ModuleVersion::Compare(ref->version, minVer) < 0)
			throw ModuleLoadException(reader.fileName, "Dependent module has insufficient version.");

		module->moduleRefs.Add(ref);
	}

	CHECKPOS_AFTER(ModuleRef);
}

void Module::ReadTypeRefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	module->typeRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->typeRefs.GetNextId(IDMASK_TYPEREF))
			throw ModuleLoadException(reader.fileName, "Invalid TypeRef token ID.");
		// Type reference has a name followed by a ModuleRef ID.
		String *typeName = module->FindString(reader.ReadToken());
		if (typeName == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID for TypeRef name.");
		TokenId modRef = reader.ReadToken();

		const Module *owner = module->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.fileName, "Unresolved ModuleRef token in TypeRef.");

		Type *type = owner->FindType(typeName, false);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef.");

		module->typeRefs.Add(const_cast<Type*>(type));
	}

	CHECKPOS_AFTER(TypeRef);
}

void Module::ReadFunctionRefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	module->functionRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->functionRefs.GetNextId(IDMASK_FUNCTIONREF))
			throw ModuleLoadException(reader.fileName, "Invalid FunctionRef token ID.");
		// Function reference has a name followed by a ModuleRef ID
		String *funcName = module->FindString(reader.ReadToken());
		if (funcName == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID for FunctionRef name.");
		TokenId modRef = reader.ReadToken();

		const Module *owner = module->FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.fileName, "Invalid module token ID in FunctionRef.");

		Method *func = owner->FindGlobalFunction(funcName, false);
		if (!func)
			throw ModuleLoadException(reader.fileName, "Unresolved FunctionRef.");

		module->functionRefs.Add(func);
	}

	CHECKPOS_AFTER(FunctionRef);
}

void Module::ReadFieldRefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	module->fieldRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->fieldRefs.GetNextId(IDMASK_FIELDREF))
			throw ModuleLoadException(reader.fileName, "Invalid FieldRef token ID.");
		// Field reference has a name followed by a TypeRef ID.
		String *fieldName = module->FindString(reader.ReadToken());
		if (fieldName == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID for FieldRef name.");
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.fileName, "FieldRef must contain a TypeRef.");

		Type *type = module->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef token in FieldRef.");

		Member *member = type->GetMember(fieldName);
		if (!member)
			throw ModuleLoadException(reader.fileName, "Unresolved FieldRef.");
		if ((member->flags & MemberFlags::FIELD) == MemberFlags::NONE)
			throw ModuleLoadException(reader.fileName, "FieldRef does not refer to a field.");

		module->fieldRefs.Add((Field*)member);
	}

	CHECKPOS_AFTER(FieldRef);
}

void Module::ReadMethodRefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	module->methodRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->methodRefs.GetNextId(IDMASK_METHODREF))
			throw ModuleLoadException(reader.fileName, "Invalid MethodRef token ID.");
		// Method reference has a name followed by a TypeRef ID.
		String *methodName = module->FindString(reader.ReadToken());
		if (methodName == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID for MethodRef name.");
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.fileName, "MethodRef must contain a TypeRef.");

		Type *type = module->FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef token in MethodRef.");

		Member *member = type->GetMember(methodName);
		if (!member)
			throw ModuleLoadException(reader.fileName, "Unresolved MethodRef.");
		if ((member->flags & MemberFlags::METHOD) == MemberFlags::NONE)
			throw ModuleLoadException(reader.fileName, "MethodRef does not refer to a method.");

		module->methodRefs.Add((Method*)member);
	}

	CHECKPOS_AFTER(MethodRef);
}

void Module::ReadStringTable(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	module->strings.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->strings.GetNextId(IDMASK_STRING))
			throw ModuleLoadException(reader.fileName, "Invalid String token ID.");

		String *value = reader.ReadString(); // GC-managed
		module->strings.Add(value);
	}

	CHECKPOS_AFTER(String);
}

void Module::ReadTypeDefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	module->types.Init(length);

	vector<FieldConstData> unresolvedConstants;

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->types.GetNextId(IDMASK_TYPEDEF))
			throw ModuleLoadException(reader.fileName, "Invalid TypeDef token ID.");

		Type *type = ReadSingleType(reader, module, id, unresolvedConstants);
		module->types.Add(type);
		module->members.Add(type->fullName, ModuleMember(type, (type->flags & TypeFlags::PRIVATE) == TypeFlags::PRIVATE));
	}

	for (auto i = unresolvedConstants.begin(); i != unresolvedConstants.end(); i++)
	{
		Type *constantType = module->FindType(i->typeId);
		if (constantType == nullptr)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef or TypeDef token ID in constant FieldDef.");
		SetConstantFieldValue(reader, module, i->field, constantType, i->value);
	}

	CHECKPOS_AFTER(TypeDef);
}

void Module::ReadFunctionDefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	module->functions.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->functions.GetNextId(IDMASK_FUNCTIONDEF))
			throw ModuleLoadException(reader.fileName, "Invalid FunctionDef token ID.");

		unique_ptr<Method> function(ReadSingleMethod(reader, module));
		function->SetDeclType(nullptr);

		if (!module->members.Add(function->name, ModuleMember(function.get(),
			(function->flags & MemberFlags::PRIVATE) == MemberFlags::PRIVATE)))
			throw ModuleLoadException(reader.fileName, "Duplicate global member name.");
		module->functions.Add(function.get());

		function.release();
	}

	CHECKPOS_AFTER(FunctionDef);
}

void Module::ReadConstantDefs(ModuleReader &reader, Module *module)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	module->constants.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->constants.GetNextId(IDMASK_CONSTANTDEF))
			throw ModuleLoadException(reader.fileName, "Invalid ConstantDef token ID.");

		enum ConstantFlags { CONST_PUBLIC = 0x01, CONST_PRIVATE = 0x02 };
		ConstantFlags flags = (ConstantFlags)reader.ReadUInt32();

		String *name = module->FindString(reader.ReadToken());
		if (name == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID in ConstantDef name.");
		TokenId typeId = reader.ReadToken();

		Type *type = module->FindType(typeId);
		if (type == nullptr)
			throw ModuleLoadException(reader.fileName, "Unresolved TypeRef or TypeDef token ID in ConstantDef.");
		if (type != VM::vm->types.String && !type->IsPrimitive())
			throw ModuleLoadException(reader.fileName, "ConstantDef type must be primitive or aves.String.");

		int64_t value = reader.ReadInt64();

		Value constant;
		constant.type = type;

		if (type == VM::vm->types.String)
		{
			String *str = module->FindString((TokenId)value);
			if (str == nullptr)
				throw ModuleLoadException(reader.fileName, "Unresolved String token ID in ConstantDef.");
			constant.common.string = str;
		}
		else
			constant.integer = value;

		module->constants.Add(constant);
		module->members.Add(name, ModuleMember(constant, (flags & CONST_PRIVATE) == CONST_PRIVATE));
	}

	CHECKPOS_AFTER(ConstantDef);
}

Type *Module::ReadSingleType(ModuleReader &reader, Module *module, const TokenId typeId, vector<FieldConstData> &unresolvedConstants)
{
	TypeFlags flags = (TypeFlags)reader.ReadUInt32();
	String *name = module->FindString(reader.ReadToken());
	if (name == nullptr)
		throw ModuleLoadException(reader.fileName, "Could not resolve string ID in TypeDef name.");

	TokenId baseTypeId   = reader.ReadToken();
	TokenId sharedTypeId = reader.ReadToken();

	Type *baseType = nullptr;
	if (baseTypeId != 0)
	{
		if (baseTypeId == typeId)
			throw ModuleLoadException(reader.fileName, "A type cannot have itself as its base type.");
		baseType = module->FindType(baseTypeId);
		if (baseType == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve base type ID.");
	}

	Type *sharedType = nullptr;
	if (sharedTypeId != 0)
	{
		if ((sharedTypeId & IDMASK_MEMBERKIND) != IDMASK_TYPEDEF)
			throw ModuleLoadException(reader.fileName, "A shared type must be a TypeDef.");
		if (sharedTypeId == typeId)
			throw ModuleLoadException(reader.fileName, "A type cannot have itself as its shared type.");
		sharedType = module->FindType(sharedTypeId);
		if (sharedType == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve shared type ID.");
	}

	const int32_t memberCount = reader.ReadInt32(); // memberCount
	unique_ptr<Type> type(new Type(memberCount));
	type->flags        = flags;
	type->baseType     = baseType;
	type->sharedType   = sharedType;
	type->fieldsOffset = baseType ? baseType->fieldsOffset + baseType->size : 0;
	type->fullName     = name;
	type->module       = module;

	ReadFields(reader, module, type.get(), unresolvedConstants);
	ReadMethods(reader, module, type.get());
	ReadProperties(reader, module, type.get());
	ReadOperators(reader, module, type.get());

	Member *instanceCtor = type->GetMember(static_strings::_new);
	if (instanceCtor && !instanceCtor->IsStatic() &&
		(instanceCtor->flags & MemberFlags::METHOD) == MemberFlags::METHOD)
		type->instanceCtor = static_cast<Method*>(instanceCtor);

	{
		unique_ptr<char[]> initer(reader.ReadCString());
		if (initer.get() != nullptr)
		{
			// Find the entry point, whoo
			TypeInitializer func = (TypeInitializer)module->FindNativeEntryPoint(initer.get());
			if (func == nullptr)
				throw ModuleLoadException(reader.fileName, "Could not locate type initializer entry point.");
			func(type.get());
		}
	}

	if (baseType && baseType->HasFinalizer())
		// This flag may already have been set by the type initializer, if there is any.
		// That's fine.
		type->flags |= TypeFlags::HAS_FINALIZER;

	TryRegisterStandardType(type.get(), module, reader);
	return type.release();
}

void Module::ReadFields(ModuleReader &reader, Module *module, Type *type, vector<FieldConstData> &unresolvedConstants)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

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
		MemberFlags flags = MemberFlags::NONE;

		if (fieldFlags & FIELD_PUBLIC)
			flags |= MemberFlags::PUBLIC;
		else if (fieldFlags & FIELD_PRIVATE)
			flags |= MemberFlags::PRIVATE;
		else if (fieldFlags & FIELD_PROTECTED)
			flags |= MemberFlags::PROTECTED;

		if (fieldFlags & FIELD_INSTANCE)
			flags |= MemberFlags::INSTANCE;

		String *name = module->FindString(reader.ReadToken());
		if (name == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID in FieldDef name.");

		unique_ptr<Field> field(new Field(name, type, flags));

		if (fieldFlags & FIELD_HASVALUE)
		{
			// The field has a constant value, gasp!
			TokenId typeId = reader.ReadToken();
			int64_t value = reader.ReadInt64();

			Type *constantType = module->FindType(typeId);
			if (!constantType)
				unresolvedConstants.push_back(FieldConstData(field.get(), typeId, value));
			else
				SetConstantFieldValue(reader, module, field.get(), constantType, value);
		}

		if (!type->members.Add(name, field.get()))
			throw ModuleLoadException(reader.fileName, "Duplicate member name in type.");
		module->fields.Add(field.get());

		if (!field->IsStatic())
		{
			field->offset = type->fieldsOffset + type->size;
			type->fieldCount++;
			type->size += sizeof(Value);
		}
		else
			field->staticValue = nullptr; // initialized only on demand

		field.release();
	}

	CHECKPOS_AFTER(FieldDef);
}

void Module::ReadMethods(ModuleReader &reader, Module *module, Type *type)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != module->methods.GetNextId(IDMASK_METHODDEF))
			throw ModuleLoadException(reader.fileName, "Invalid MethodDef token ID.");

		unique_ptr<Method> method(ReadSingleMethod(reader, module));

		if (!type->members.Add(method->name, method.get()))
			throw ModuleLoadException(reader.fileName, "Duplicate member name in type.");
		module->methods.Add(method.get());
		method->SetDeclType(type);

		// If this method is not private and the base type is not null,
		// see if any base type declares a public or protected method
		// with the same name, and if so, update this method's baseMethod
		// to that value.
		// Oh, and, we don't run this step if the name is one of '.new',
		// '.iter' or '.init'. Other dot-methods (including '.call') are
		// insufficiently special to be excluded.
		if (type->baseType != nullptr &&
			(method->flags & MemberFlags::ACCESS_LEVEL) != MemberFlags::PRIVATE &&
			!String_Equals(method->name, static_strings::_new) &&
			!String_Equals(method->name, static_strings::_iter) &&
			!String_Equals(method->name, static_strings::_init))
		{
			Type *t = type->baseType;
			do
			{
				Member *m;
				if (m = t->GetMember(method->name))
				{
					// The two members are considered matching if:
					//   1. they have the same accessibility
					//   2. they are both either static or instance methods
					//   3. they are both methods
					const MemberFlags matchingFlags = MemberFlags::KIND |
						MemberFlags::ACCESS_LEVEL | MemberFlags::INSTANCE;
					if ((m->flags & matchingFlags) == (method->flags & matchingFlags))
						method->baseMethod = static_cast<Method*>(m);
					break;
				}
			} while (t = t->baseType);
		}

		method.release();
	}

	CHECKPOS_AFTER(MethodDef);
}

void Module::ReadProperties(ModuleReader &reader, Module *module, Type *type)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	for (int32_t i = 0; i < length; i++)
	{
		String *name = module->FindString(reader.ReadToken());
		if (name == nullptr)
			throw ModuleLoadException(reader.fileName, "Could not resolve string ID in property name.");
		TokenId getterId = reader.ReadToken();
		TokenId setterId = reader.ReadToken();

		MemberFlags flags = MemberFlags::NONE;
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

			flags = getter->flags & ~(MemberFlags::IMPL | MemberFlags::KIND);
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

			MemberFlags setterFlags = setter->flags & ~(MemberFlags::IMPL | MemberFlags::KIND);
			if (flags != MemberFlags::NONE && setterFlags != flags)
				throw ModuleLoadException(reader.fileName, "Property getter and setter must have the same accessibility, and matching abstract, virtual, sealed and instance flags.");

			// We've just determined that either the flags the same, or 'flags' is empty,
			// so overwriting here is fine.
			flags = setterFlags;
		}

		if (!getter && !setter)
			throw ModuleLoadException(reader.fileName, "Property must have at least one accessor.");

		unique_ptr<Property> prop(new Property(name, type, flags));
		prop->getter = getter;
		prop->setter = setter;

		if (!type->members.Add(prop->name, prop.get()))
			throw ModuleLoadException(reader.fileName, "Duplicate member name in type.");

		prop.release();
	}

	CHECKPOS_AFTER(PropertyDef);
}

void Module::ReadOperators(ModuleReader &reader, Module *module, Type *type)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	if (length > 0)
	{
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
			if (type->operators[(int)op] != nullptr)
				throw ModuleLoadException(reader.fileName, "Duplicate operator declaration.");
			Method::Overload *mo = method->ResolveOverload(Arity(op));
			if (!mo)
				throw ModuleLoadException(reader.fileName, "Operator method must have an overload for the operator.");

			type->operators[(int)op] = mo;
		}
	}

	CHECKPOS_AFTER(OperatorDef);

	type->InitOperators();
}

void Module::SetConstantFieldValue(ModuleReader &reader, Module *module, Field *field, Type *constantType, const int64_t value)
{
	if (constantType != VM::vm->types.String && !constantType->IsPrimitive())
		throw ModuleLoadException(reader.fileName, "Constant type in FieldDef must be primitive or aves.String.");
				
	Value constantValue;
	constantValue.type = constantType;

	if (constantType == VM::vm->types.String)
	{
		String *str = module->FindString((TokenId)value);
		if (!str)
			throw ModuleLoadException(reader.fileName, "Unresolved String token ID in constant FieldDef.");
		constantValue.common.string = str;
	}
	else
		constantValue.integer = value;

	field->staticValue = GC::gc->AddStaticReference(constantValue);
}

Method *Module::ReadSingleMethod(ModuleReader &reader, Module *module)
{
	FileMethodFlags methodFlags = (FileMethodFlags)reader.ReadUInt32();

	String *name = module->FindString(reader.ReadToken());
	if (name == nullptr)
		throw ModuleLoadException(reader.fileName, "Could not resolve string ID in MethodDef or FunctionDef name.");

	const uint32_t size = reader.ReadUInt32();
	if (size == 0)
		throw ModuleLoadException(reader.fileName, "Method found without overloads.");
	
	const long posBefore = reader.GetPosition();
	const int32_t overloadCount = reader.ReadInt32();

	if (overloadCount == 0)
		throw ModuleLoadException(reader.fileName, "Method found without overloads.");

	MemberFlags memberFlags = MemberFlags::NONE;
	if (methodFlags & FM_PUBLIC)
		memberFlags |= MemberFlags::PUBLIC;
	else if (methodFlags & FM_PRIVATE)
		memberFlags |= MemberFlags::PRIVATE;
	else if (methodFlags & FM_PROTECTED)
		memberFlags |= MemberFlags::PROTECTED;
	if (methodFlags & FM_INSTANCE)
		memberFlags |= MemberFlags::INSTANCE;
	if (methodFlags & FM_IMPL)
		memberFlags |= MemberFlags::IMPL;

	unique_ptr<Method> method(new Method(name, module, memberFlags));

	unique_ptr<Method::Overload[]> overloads(new Method::Overload[overloadCount]);
	memset(overloads.get(), 0, overloadCount * sizeof(Method::Overload));

	for (int32_t i = 0; i < overloadCount; i++)
	{
		OverloadFlags flags = (OverloadFlags)reader.ReadUInt32();

		Method::Overload *ov = overloads.get() + i;
		ov->group = method.get();

		// Parameter count & names
		uint16_t paramCount = reader.ReadUInt16();
		ov->paramCount = paramCount;
		ov->paramNames = new String*[paramCount];
		{
			int instOffset = methodFlags & FM_INSTANCE ? 1 : 0;
			RefSignatureBuilder refBuilder(paramCount + instOffset);

			for (int p = 0; p < paramCount; p++)
			{
				enum ParamFlags : uint16_t { PF_BY_REF = 0x0001 };
				TokenId paramNameId = reader.ReadToken();
				ParamFlags paramFlags = (ParamFlags)reader.ReadUInt16();
				ov->paramNames[p] = module->FindString(paramNameId);
				if (paramFlags == PF_BY_REF)
					refBuilder.SetParam(p + instOffset, true);
			}

			ov->refSignature = refBuilder.Commit();
		}

		// Flags
		ov->flags = (MethodFlags)0;
		if (methodFlags & FM_CTOR)
			ov->flags |= MethodFlags::CTOR;
		if (methodFlags & FM_INSTANCE)
			ov->flags |= MethodFlags::INSTANCE;
		if (flags & OV_VAREND)
			ov->flags |= MethodFlags::VAR_END;
		if (flags & OV_VARSTART)
			ov->flags |= MethodFlags::VAR_START;
		if (flags & OV_VIRTUAL)
			ov->flags |= MethodFlags::VIRTUAL;
		if (flags & OV_ABSTRACT)
			ov->flags |= MethodFlags::ABSTRACT;

		// Header
		{
			int32_t tryCount = 0;
			unique_ptr<Method::TryBlock[]> tries(nullptr);
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
				tries = unique_ptr<Method::TryBlock[]>(ReadTryBlocks(reader, module, tryCount));
			}

			ov->tryBlockCount = tryCount;
			ov->tryBlocks = tries.release();
		}

		// Body
		if (!(flags & OV_ABSTRACT))
		{
			if (flags & OV_NATIVE)
			{
				unique_ptr<char[]> entryPointName(reader.ReadCString());
				NativeMethod entryPoint = (NativeMethod)module->FindNativeEntryPoint(entryPointName.get());
				if (entryPoint == nullptr)
					throw ModuleLoadException(reader.fileName, "Could not locate entry point of native method.");
				ov->nativeEntry = entryPoint;
				ov->flags |= MethodFlags::NATIVE;
			}
			else
			{
				uint32_t offset = reader.ReadUInt32(); // The offset of the first instruction in the method, relative to the method block
				uint32_t length = reader.ReadUInt32(); // The length of the body, in bytes

				const long posCurrent = reader.GetPosition(); // Resumption point

				// Read the method body
				reader.Seek(module->methodStart + offset, SeekOrigin::BEGIN);
				unique_ptr<uint8_t[]> body(new uint8_t[length]);
				reader.Read(body.get(), length);

				// Return to previous position
				reader.Seek(posCurrent, SeekOrigin::BEGIN);

				ov->length = length;
				ov->entry = body.release();
			}
		}
	}

	const long posAfter = reader.GetPosition();
	if (posBefore + (long)size != posAfter)
		throw ModuleLoadException(reader.fileName, "The actual size of the overloads table did not match the expected size.");

	method->overloadCount = overloadCount;
	method->overloads = overloads.release();

	return method.release();
}

Method::TryBlock *Module::ReadTryBlocks(ModuleReader &reader, Module *module, int32_t &tryCount)
{
	typedef Method::TryBlock::TryKind TryKind;
	unique_ptr<Method::TryBlock[]> output(nullptr);

	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	output = unique_ptr<Method::TryBlock[]>(new Method::TryBlock[length]);
	Method::TryBlock *tries = output.get();

	for (int32_t i = 0; i < length; i++)
	{
		Method::TryBlock::TryKind kind = (Method::TryBlock::TryKind)reader.ReadUInt8();
		uint32_t tryStart = reader.ReadUInt32();
		uint32_t tryEnd   = reader.ReadUInt32();

		Method::TryBlock *curTry = tries + i;
		*curTry = Method::TryBlock(kind, tryStart, tryEnd);

		switch (kind)
		{
		case TryKind::FINALLY:
			curTry->finallyBlock.finallyStart = reader.ReadUInt32();
			curTry->finallyBlock.finallyEnd = reader.ReadUInt32();
			break;
		case TryKind::CATCH:
			{
				uint32_t catchSize = reader.ReadUInt32();
				if (catchSize != 0)
				{
					int32_t catchLength = reader.ReadInt32();
					unique_ptr<Method::CatchBlock[]> catches(new Method::CatchBlock[catchLength]);

					for (int32_t i = 0; i < catchLength; i++)
					{
						Method::CatchBlock *curCatch = catches.get() + i;

						curCatch->caughtTypeId = reader.ReadToken();
						// Try to resolve the type right away. If it fails, do it when the method
						// is initialized instead.
						if (module->FindType(curCatch->caughtTypeId))
							curCatch->caughtType = module->FindType(curCatch->caughtTypeId);
						else
							curCatch->caughtType = nullptr;

						curCatch->catchStart = reader.ReadUInt32();
						curCatch->catchEnd   = reader.ReadUInt32();
					}

					curTry->catches.count = catchLength;
					curTry->catches.blocks = catches.release();
				}
			}
			break;
		}
	}

	tryCount = length;

	CHECKPOS_AFTER(tries);

	return output.release();
}

void Module::TryRegisterStandardType(Type *type, Module *fromModule, ModuleReader &reader)
{
	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType stdType = std_type_names::Types[i];
		if (String_Equals(type->fullName, stdType.name))
		{
			if (VM::vm->types.*(stdType.member) == nullptr)
			{
				VM::vm->types.*(stdType.member) = type;
				if (stdType.initerFunction)
				{
					void *func = fromModule->FindNativeEntryPoint(stdType.initerFunction);
					if (!func)
						throw ModuleLoadException(reader.fileName, "Missing instance initializer for standard type in native library.");

					// Can't really switch here :(
					// Also because all initializer functions are of different types,
					// we can't really store a VM::IniterFunctions member in stdType.
					if (type == VM::vm->types.List)
						VM::vm->functions.initListInstance = (ListInitializer)func;
					else if (type == VM::vm->types.Hash)
						VM::vm->functions.initHashInstance = (HashInitializer)func;
					else if (type == VM::vm->types.Type)
						VM::vm->functions.initTypeToken = (TypeTokenInitializer)func;
				}
			}
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

OVUM_API bool Module_FindConstant(ModuleHandle module, String *name, bool includeInternal, Value &result)
{
	return module->FindConstant(name, includeInternal, result);
}

OVUM_API void *Module_FindNativeFunction(ModuleHandle module, const char *name)
{
	return module->FindNativeFunction(name);
}