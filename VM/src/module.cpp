#include "module.h"
#include "modulereader.h"
#include "stringbuffer.h"
#include "debug_symbols.h"
#include "refsignature.h"
#include <memory>

namespace ovum
{

const char *const Module::NativeModuleIniterName = "OvumModuleMain";

namespace module_file
{
	// The magic number that must be present in all Ovum modules.
	static const char MagicNumber[] = { 'O', 'V', 'M', 'M' };

	// The start of the "real" data in the module.
	static const unsigned int DataStart = 16;

	// The minimum supported file format version
	static const uint32_t MinFileFormatVersion = 0x00000100u;

	// The maximum supported file format version
	static const uint32_t MaxFileFormatVersion = 0x00000100u;

	// The file extension
	static const pathchar_t *const Extension = _Path(".ovm");
}

Module::Module(uint32_t fileFormatVersion, ModuleMeta &meta, const PathName &fileName, VM *vm) :
	// This initializer list is kind of silly
	fileFormatVersion(fileFormatVersion),
	name(meta.name),
	version(meta.version),
	fileName(fileName),
	fullyOpened(false),
	// defs
	functions(meta.functionCount),
	types(meta.typeCount),
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
	debugData(nullptr),
	vm(vm),
	pool(vm->GetModulePool())
{ }

Module::~Module()
{
	// Note: Don't touch any of the string values.
	// They're managed by the GC, so we let her clean it up.

	functions.DeleteEntries();
	fields.DeleteEntries();
	methods.DeleteEntries();
	types.DeleteEntries();

	//members.DeleteValues(); // Nope, these values are not pointers

	// Don't delete the refs here! They are in their own modules.

	FreeNativeLibrary();

	delete debugData;

	// If the module is not fullyOpened, then the module is being deallocated from
	// an exception in Module::Open, so we must remove it from the pool again.
	if (!fullyOpened && pool)
		pool->Remove(this);
}

Module *Module::FindModuleRef(String *name) const
{
	for (int32_t i = 0; i < moduleRefs.GetLength(); i++)
		if (String_Equals(moduleRefs[i]->name, name))
			return moduleRefs[i];

	return nullptr;
}

bool Module::FindMember(String *name, bool includeInternal, ModuleMember &result) const
{
	ModuleMember member;
	if (!members.Get(name, member))
		return false;
	
	if (!includeInternal && (member.flags & ModuleMemberFlags::PROTECTION) == ModuleMemberFlags::INTERNAL)
		return false;

	result = member;
	return true;
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


Module *Module::Open(VM *vm, const PathName &fileName, ModuleVersion *requiredVersion)
{
	Module *outputModule = nullptr;

	try
	{
		ModuleReader reader(vm);
		reader.Open(fileName);
		VerifyMagicNumber(reader);
		uint32_t fileFormatVersion = reader.ReadUInt32();
		if (fileFormatVersion < module_file::MinFileFormatVersion ||
			fileFormatVersion > module_file::MaxFileFormatVersion)
			throw ModuleLoadException(fileName, "Unsupported module file format version.");

		reader.Seek(module_file::DataStart, os::FILE_SEEK_START);

		ModuleMeta meta;
		ReadModuleMeta(reader, meta);

		// Check whether we have the right version before allocating the Module object.
		// If the version doesn't match, we don't actually need to continue reading.
		if (requiredVersion && meta.version != *requiredVersion)
			throw ModuleLoadException(fileName, "Dependent module has the wrong version.");

		// ReadModuleMeta gives us just enough information to initialize the output module
		// and add it to the list of loaded modules.
		// It's not fully loaded yet, but we add it specifically so that we can detect
		// circular dependencies.
		std::unique_ptr<Module> output(new Module(fileFormatVersion, meta, fileName, vm));
		vm->GetModulePool()->Add(output.get());

		if (meta.nativeLib)
			output->LoadNativeLibrary(meta.nativeLib, fileName);

		output->ReadStringTable(reader);  // strings

		// And these must be called in exactly this order!
		output->ReadModuleRefs(reader);   // moduleRefs
		output->ReadTypeRefs(reader);     // typeRefs
		output->ReadFunctionRefs(reader); // functionRefs
		output->ReadFieldRefs(reader);    // fieldRefs
		output->ReadMethodRefs(reader);   // methodRefs

		output->ReadTypeDefs(reader);     // types
		output->ReadFunctionDefs(reader); // functions
		output->ReadConstantDefs(reader, meta.constantCount); // constants

		TokenId mainMethodId = reader.ReadToken();
		if (mainMethodId != 0)
		{
			if ((mainMethodId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF &&
				(mainMethodId & IDMASK_MEMBERKIND) != IDMASK_FUNCTIONDEF)
				throw ModuleLoadException(reader.GetFileName(), "Main method token ID must be a MethodDef or FunctionDef.");

			Method *mainMethod = output->FindMethod(mainMethodId);
			if (mainMethod == nullptr)
				throw ModuleLoadException(reader.GetFileName(), "Unresolved main method token ID.");
			if ((mainMethod->flags & MemberFlags::INSTANCE) != MemberFlags::NONE)
				throw ModuleLoadException(reader.GetFileName(), "Main method cannot be an instance method.");

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
		throw ModuleLoadException(fileName, ioError.what());
	}
	catch (std::bad_alloc&)
	{
		throw ModuleLoadException(fileName, "Out of memory");
	}

	outputModule->fullyOpened = true;

	return outputModule;
}

Module *Module::OpenByName(VM *vm, String *name, ModuleVersion *requiredVersion)
{
	Module *output;
	if (output = vm->GetModulePool()->Get(name, requiredVersion))
		return output;
	
	PathName versionNumber(32);
	if (requiredVersion)
		AppendVersionString(versionNumber, *requiredVersion);

	PathName moduleFileName(256);

	static const int pathCount = 3;
	const PathName *paths[pathCount] = {
		vm->startupPathLib,
		vm->startupPath,
		vm->modulePath,
	};

	bool found = false;
	for (int i = 0; i < pathCount; i++)
	{
		moduleFileName.ReplaceWith(*paths[i]);
		uint32_t simpleName = moduleFileName.Join(name);
		// Versioned names first:
		//    path/$name-$version/$name.ovm
		//    path/$name-$version.ovm
		if (requiredVersion) {
			moduleFileName.Append(_Path("-"));
			// The length for path/$name-$version
			uint32_t versionedName = moduleFileName.Append(versionNumber);

			// path/$name-version/$name.ovm
			moduleFileName.Join(name);
			moduleFileName.Append(module_file::Extension);
			if (found = os::FileExists(moduleFileName.GetDataPointer()))
				break;

			// path/$name-$version.ovm
			moduleFileName.ClipTo(0, versionedName);
			moduleFileName.Append(module_file::Extension);
			if (found = os::FileExists(moduleFileName.GetDataPointer()))
				break;
		}

		// Then, unversioned names:
		//    path/$name/$name.ovm
		//    path/$name.ovm
		// simpleName contains the length for path/$name

		// path/$name/$name.ovm
		moduleFileName.ClipTo(0, simpleName);
		moduleFileName.Join(name);
		moduleFileName.Append(module_file::Extension);
		if (found = os::FileExists(moduleFileName.GetDataPointer()))
			break;

		// path/$name.ovm
		moduleFileName.ClipTo(0, simpleName);
		moduleFileName.Append(module_file::Extension);
		if (found = os::FileExists(moduleFileName.GetDataPointer()))
			break;
	}

	if (!found)
	{
		moduleFileName.ReplaceWith(name);
		throw ModuleLoadException(moduleFileName, "Could not locate the module file.");
	}

	if (vm->verbose)
	{
		VM::Printf(L"Loading module '%ls' ", name);
		wprintf(L"from file '" PATHNWF L"'\n", moduleFileName.GetDataPointer());
	}

	output = Open(vm, moduleFileName, requiredVersion);

	if (vm->verbose)
		VM::Printf(L"Successfully loaded module '%ls'\n", name);

	return output;
}

void Module::LoadNativeLibrary(String *nativeFileName, const PathName &path)
{
	// Native library files are ALWAYS loaded from the same folder
	// as the module file. Immer & mindig. 'path' contains the full
	// path and file name of the module file, so we strip the module
	// file name and append nativeFileName! Simple!

	PathName fileName(path);
	fileName.RemoveFileName();
	fileName.Join(nativeFileName);

	// fileName should now contain a full path to the native module
	this->nativeLib = LoadLibraryW(fileName.GetDataPointer());

	// If this->nativeLib is null, then the library could not be loaded.
	if (!this->nativeLib)
		throw ModuleLoadException(path, "Could not load native library file.");
}

void *Module::FindNativeEntryPoint(const char *name) const
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
			throw ModuleLoadException(reader.GetFileName(), "Invalid magic number in file.");
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
			throw ModuleLoadException(reader.GetFileName(), "The actual size of the " #tbl " table did not match the expected size."); \
	}
#define CHECKPOS_AFTER(tbl) CHECKPOS_AFTER_(tbl)

void Module::ReadModuleRefs(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	moduleRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != moduleRefs.GetNextId(IDMASK_MODULEREF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid ModuleRef token ID.");

		// Module reference has a name followed by a version
		String *modName = FindString(reader.ReadToken());
		if (modName == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID for ModuleRef name.");

		ModuleVersion version;
		ReadVersion(reader, version);

		Module *ref = OpenByName(vm, modName, &version);
		if (!ref->fullyOpened)
			throw ModuleLoadException(reader.GetFileName(), "Circular dependency detected.");
		if (ref->version != version)
			throw ModuleLoadException(reader.GetFileName(), "Dependent module has the wrong version.");

		moduleRefs.Add(ref);
	}

	CHECKPOS_AFTER(ModuleRef);
}

void Module::ReadTypeRefs(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	typeRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != typeRefs.GetNextId(IDMASK_TYPEREF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid TypeRef token ID.");
		// Type reference has a name followed by a ModuleRef ID.
		String *typeName = FindString(reader.ReadToken());
		if (typeName == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID for TypeRef name.");
		TokenId modRef = reader.ReadToken();

		const Module *owner = FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved ModuleRef token in TypeRef.");

		Type *type = owner->FindType(typeName, false);
		if (!type)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved TypeRef.");

		typeRefs.Add(const_cast<Type*>(type));
	}

	CHECKPOS_AFTER(TypeRef);
}

void Module::ReadFunctionRefs(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	functionRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != functionRefs.GetNextId(IDMASK_FUNCTIONREF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid FunctionRef token ID.");
		// Function reference has a name followed by a ModuleRef ID
		String *funcName = FindString(reader.ReadToken());
		if (funcName == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID for FunctionRef name.");
		TokenId modRef = reader.ReadToken();

		const Module *owner = FindModuleRef(modRef);
		if (!owner)
			throw ModuleLoadException(reader.GetFileName(), "Invalid module token ID in FunctionRef.");

		Method *func = owner->FindGlobalFunction(funcName, false);
		if (!func)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved FunctionRef.");

		functionRefs.Add(func);
	}

	CHECKPOS_AFTER(FunctionRef);
}

void Module::ReadFieldRefs(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	fieldRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != fieldRefs.GetNextId(IDMASK_FIELDREF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid FieldRef token ID.");
		// Field reference has a name followed by a TypeRef ID.
		String *fieldName = FindString(reader.ReadToken());
		if (fieldName == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID for FieldRef name.");
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.GetFileName(), "FieldRef must contain a TypeRef.");

		Type *type = FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved TypeRef token in FieldRef.");

		Member *member = type->GetMember(fieldName);
		if (!member)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved FieldRef.");
		if ((member->flags & MemberFlags::FIELD) == MemberFlags::NONE)
			throw ModuleLoadException(reader.GetFileName(), "FieldRef does not refer to a field.");

		fieldRefs.Add((Field*)member);
	}

	CHECKPOS_AFTER(FieldRef);
}

void Module::ReadMethodRefs(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();
	methodRefs.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != methodRefs.GetNextId(IDMASK_METHODREF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid MethodRef token ID.");
		// Method reference has a name followed by a TypeRef ID.
		String *methodName = FindString(reader.ReadToken());
		if (methodName == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID for MethodRef name.");
		TokenId typeRef = reader.ReadToken();

		if ((typeRef & IDMASK_MEMBERKIND) != IDMASK_TYPEREF)
			throw ModuleLoadException(reader.GetFileName(), "MethodRef must contain a TypeRef.");

		Type *type = FindType(typeRef);
		if (!type)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved TypeRef token in MethodRef.");

		Member *member = type->GetMember(methodName);
		if (!member)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved MethodRef.");
		if ((member->flags & MemberFlags::METHOD) == MemberFlags::NONE)
			throw ModuleLoadException(reader.GetFileName(), "MethodRef does not refer to a method.");

		methodRefs.Add((Method*)member);
	}

	CHECKPOS_AFTER(MethodRef);
}

void Module::ReadStringTable(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	strings.Init(length);

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != strings.GetNextId(IDMASK_STRING))
			throw ModuleLoadException(reader.GetFileName(), "Invalid String token ID.");

		String *value = reader.ReadString(); // GC-managed
		strings.Add(value);
	}

	CHECKPOS_AFTER(String);
}

void Module::ReadTypeDefs(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	if (length != types.GetCapacity())
		throw ModuleLoadException(reader.GetFileName(), "Length of TypeDef table differs from typeCount in module header.");

	std::vector<FieldConstData> unresolvedConstants;

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != types.GetNextId(IDMASK_TYPEDEF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid TypeDef token ID.");

		Type *type = ReadSingleType(reader, id, unresolvedConstants);
		types.Add(type);
		members.Add(type->fullName, ModuleMember(type, (type->flags & TypeFlags::PRIVATE) == TypeFlags::PRIVATE));
	}

	for (auto i = unresolvedConstants.begin(); i != unresolvedConstants.end(); i++)
	{
		Type *constantType = FindType(i->typeId);
		if (constantType == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved TypeRef or TypeDef token ID in constant FieldDef.");
		SetConstantFieldValue(reader, i->field, constantType, i->value);
	}

	CHECKPOS_AFTER(TypeDef);
}

void Module::ReadFunctionDefs(ModuleReader &reader)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	if (length != functions.GetCapacity())
		throw ModuleLoadException(reader.GetFileName(), "Length of FunctionDef table differs from functionCount in module header.");

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != functions.GetNextId(IDMASK_FUNCTIONDEF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid FunctionDef token ID.");

		std::unique_ptr<Method> function(ReadSingleMethod(reader));
		function->SetDeclType(nullptr);

		if (!members.Add(function->name, ModuleMember(function.get(),
			(function->flags & MemberFlags::PRIVATE) == MemberFlags::PRIVATE)))
			throw ModuleLoadException(reader.GetFileName(), "Duplicate global member name.");
		functions.Add(function.get());

		function.release();
	}

	CHECKPOS_AFTER(FunctionDef);
}

void Module::ReadConstantDefs(ModuleReader &reader, int32_t headerConstantCount)
{
	CHECKPOS_BEFORE();

	int32_t length = reader.ReadInt32();
	if (length != headerConstantCount)
		throw ModuleLoadException(reader.GetFileName(), "Length of ConstantDef table differs from constantCount in module header.");

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != (IDMASK_CONSTANTDEF | (i + 1)))
			throw ModuleLoadException(reader.GetFileName(), "Invalid ConstantDef token ID.");

		ConstantFlags flags = reader.Read<ConstantFlags>();

		String *name = FindString(reader.ReadToken());
		if (name == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID in ConstantDef name.");
		TokenId typeId = reader.ReadToken();

		Type *type = FindType(typeId);
		if (type == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved TypeRef or TypeDef token ID in ConstantDef.");
		if (type != vm->types.String && !type->IsPrimitive())
			throw ModuleLoadException(reader.GetFileName(), "ConstantDef type must be primitive or aves.String.");

		int64_t value = reader.ReadInt64();

		Value constant;
		constant.type = type;

		if (type == vm->types.String)
		{
			String *str = FindString((TokenId)value);
			if (str == nullptr)
				throw ModuleLoadException(reader.GetFileName(), "Unresolved String token ID in ConstantDef.");
			constant.common.string = str;
		}
		else
			constant.integer = value;

		members.Add(name, ModuleMember(name, constant, (flags & CONST_PRIVATE) == CONST_PRIVATE));
	}

	CHECKPOS_AFTER(ConstantDef);
}

Type *Module::ReadSingleType(ModuleReader &reader, const TokenId typeId,
                             std::vector<FieldConstData> &unresolvedConstants)
{
	TypeFlags flags = reader.Read<TypeFlags>();
	String *name = FindString(reader.ReadToken());
	if (name == nullptr)
		throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID in TypeDef name.");

	TokenId baseTypeId   = reader.ReadToken();
	TokenId sharedTypeId = reader.ReadToken();

	Type *baseType = nullptr;
	if (baseTypeId != 0)
	{
		if (baseTypeId == typeId)
			throw ModuleLoadException(reader.GetFileName(), "A type cannot have itself as its base type.");
		baseType = FindType(baseTypeId);
		if (baseType == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve base type ID.");
	}

	Type *sharedType = nullptr;
	if (sharedTypeId != 0)
	{
		if ((sharedTypeId & IDMASK_MEMBERKIND) != IDMASK_TYPEDEF)
			throw ModuleLoadException(reader.GetFileName(), "A shared type must be a TypeDef.");
		if (sharedTypeId == typeId)
			throw ModuleLoadException(reader.GetFileName(), "A type cannot have itself as its shared type.");
		sharedType = FindType(sharedTypeId);
		if (sharedType == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve shared type ID.");
	}

	const int32_t memberCount = reader.ReadInt32(); // memberCount
	std::unique_ptr<Type> type(new Type(this, memberCount));
	type->flags        = flags;
	type->baseType     = baseType;
	type->sharedType   = sharedType;
	type->fieldsOffset = baseType ? baseType->GetTotalSize() : 0;
	type->fullName     = name;

	ReadFields(reader, type.get(), unresolvedConstants);
	ReadMethods(reader, type.get());
	ReadProperties(reader, type.get());
	ReadOperators(reader, type.get());

	Member *instanceCtor = type->GetMember(static_strings::_new);
	if (instanceCtor && !instanceCtor->IsStatic() &&
		(instanceCtor->flags & MemberFlags::METHOD) == MemberFlags::METHOD)
		type->instanceCtor = static_cast<Method*>(instanceCtor);

	{
		std::unique_ptr<char[]> initer(reader.ReadCString());
		if (initer.get() != nullptr)
		{
			// Find the entry point, whoo
			TypeInitializer func = (TypeInitializer)FindNativeEntryPoint(initer.get());
			if (func == nullptr)
				throw ModuleLoadException(reader.GetFileName(), "Could not locate type initializer entry point.");
			func(type.get());
		}
	}

	if (baseType && baseType->HasFinalizer())
		// This flag may already have been set by the type initializer, if there is any.
		// That's fine.
		type->flags |= TypeFlags::HAS_FINALIZER;

	TryRegisterStandardType(type.get(), reader);
	return type.release();
}

void Module::ReadFields(ModuleReader &reader, Type *type,
                        std::vector<FieldConstData> &unresolvedConstants)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != fields.GetNextId(IDMASK_FIELDDEF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid FieldDef token ID.");

		FieldFlags fieldFlags = reader.Read<FieldFlags>();
		if ((fieldFlags & FIELD_HASVALUE) && (fieldFlags & FIELD_INSTANCE))
			throw ModuleLoadException(reader.GetFileName(), "The field flags hasValue and instance cannot be used together.");
		MemberFlags flags = MemberFlags::NONE;

		if (fieldFlags & FIELD_PUBLIC)
			flags |= MemberFlags::PUBLIC;
		else if (fieldFlags & FIELD_PRIVATE)
			flags |= MemberFlags::PRIVATE;
		else if (fieldFlags & FIELD_PROTECTED)
			flags |= MemberFlags::PROTECTED;

		if (fieldFlags & FIELD_INSTANCE)
			flags |= MemberFlags::INSTANCE;

		String *name = FindString(reader.ReadToken());
		if (name == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID in FieldDef name.");

		std::unique_ptr<Field> field(new Field(name, type, flags));

		if (fieldFlags & FIELD_HASVALUE)
		{
			// The field has a constant value, gasp!
			TokenId typeId = reader.ReadToken();
			int64_t value = reader.ReadInt64();

			Type *constantType = FindType(typeId);
			if (!constantType)
				unresolvedConstants.push_back(FieldConstData(field.get(), typeId, value));
			else
				SetConstantFieldValue(reader, field.get(), constantType, value);
		}

		if (!type->members.Add(name, field.get()))
			throw ModuleLoadException(reader.GetFileName(), "Duplicate member name in type.");
		fields.Add(field.get());

		if (!field->IsStatic())
		{
			field->offset = type->GetTotalSize();
			type->fieldCount++;
			type->size += sizeof(Value);
		}
		else
			field->staticValue = nullptr; // initialized only on demand

		field.release();
	}

	CHECKPOS_AFTER(FieldDef);
}

void Module::ReadMethods(ModuleReader &reader, Type *type)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	for (int32_t i = 0; i < length; i++)
	{
		TokenId id = reader.ReadToken();
		if (id != methods.GetNextId(IDMASK_METHODDEF))
			throw ModuleLoadException(reader.GetFileName(), "Invalid MethodDef token ID.");

		std::unique_ptr<Method> method(ReadSingleMethod(reader));

		if (!type->members.Add(method->name, method.get()))
			throw ModuleLoadException(reader.GetFileName(), "Duplicate member name in type.");
		methods.Add(method.get());
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

void Module::ReadProperties(ModuleReader &reader, Type *type)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	for (int32_t i = 0; i < length; i++)
	{
		String *name = FindString(reader.ReadToken());
		if (name == nullptr)
			throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID in property name.");
		TokenId getterId = reader.ReadToken();
		TokenId setterId = reader.ReadToken();

		MemberFlags flags = MemberFlags::NONE;
		Method *getter = nullptr;
		if (getterId != 0)
		{
			if ((getterId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF)
				throw ModuleLoadException(reader.GetFileName(), "Property getter must be a MethodDef.");
			getter = FindMethod(getterId);
			if (!getter)
				throw ModuleLoadException(reader.GetFileName(), "Unresolved MethodDef token ID in property getter.");
			if (getter->declType != type)
				throw ModuleLoadException(reader.GetFileName(), "Property getter must refer to a method in the same type as the property.");

			flags = getter->flags & ~(MemberFlags::IMPL | MemberFlags::KIND);
		}

		Method *setter = nullptr;
		if (setterId != 0)
		{
			if ((setterId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF)
				throw ModuleLoadException(reader.GetFileName(), "Property setter must be a MethodDef.");
			setter = FindMethod(setterId);
			if (!setter)
				throw ModuleLoadException(reader.GetFileName(), "Unresolved MethodDef token ID in property setter.");
			if (setter->declType != type)
				throw ModuleLoadException(reader.GetFileName(), "Property setter must refer to a method in the same type as the property.");

			MemberFlags setterFlags = setter->flags & ~(MemberFlags::IMPL | MemberFlags::KIND);
			if (flags != MemberFlags::NONE && setterFlags != flags)
				throw ModuleLoadException(reader.GetFileName(), "Property getter and setter must have the same accessibility, and matching abstract, virtual, sealed and instance flags.");

			// We've just determined that either the flags the same, or 'flags' is empty,
			// so overwriting here is fine.
			flags = setterFlags;
		}

		if (!getter && !setter)
			throw ModuleLoadException(reader.GetFileName(), "Property must have at least one accessor.");

		std::unique_ptr<Property> prop(new Property(name, type, flags));
		prop->getter = getter;
		prop->setter = setter;

		if (!type->members.Add(prop->name, prop.get()))
			throw ModuleLoadException(reader.GetFileName(), "Duplicate member name in type.");

		prop.release();
	}

	CHECKPOS_AFTER(PropertyDef);
}

void Module::ReadOperators(ModuleReader &reader, Type *type)
{
	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	if (length > 0)
	{
		for (int32_t i = 0; i < length; i++)
		{
			Operator op = reader.Read<Operator>();
			TokenId methodId = reader.ReadToken();

			if ((methodId & IDMASK_MEMBERKIND) != IDMASK_METHODDEF)
				throw ModuleLoadException(reader.GetFileName(), "Operator method must be a MethodDef.");
			Method *method = FindMethod(methodId);
			if (!method)
				throw ModuleLoadException(reader.GetFileName(), "Unresolved MethodDef token ID in operator.");
			if (method->declType != type)
				throw ModuleLoadException(reader.GetFileName(), "Operator method must be in the same type as the property.");
			if (type->operators[(int)op] != nullptr)
				throw ModuleLoadException(reader.GetFileName(), "Duplicate operator declaration.");
			MethodOverload *mo = method->ResolveOverload(Arity(op));
			if (!mo)
				throw ModuleLoadException(reader.GetFileName(), "Operator method must have an overload for the operator.");

			type->operators[(int)op] = mo;
		}
	}

	CHECKPOS_AFTER(OperatorDef);

	type->InitOperators();
}

void Module::SetConstantFieldValue(ModuleReader &reader, Field *field, Type *constantType, const int64_t value)
{
	if (constantType != vm->types.String && !constantType->IsPrimitive())
		throw ModuleLoadException(reader.GetFileName(), "Constant type in FieldDef must be primitive or aves.String.");
				
	Value constantValue;
	constantValue.type = constantType;

	if (constantType == vm->types.String)
	{
		String *str = FindString((TokenId)value);
		if (!str)
			throw ModuleLoadException(reader.GetFileName(), "Unresolved String token ID in constant FieldDef.");
		constantValue.common.string = str;
	}
	else
		constantValue.integer = value;

	field->staticValue = GetGC()->AddStaticReference(nullptr, constantValue);
	if (!field->staticValue)
		throw ModuleLoadException(reader.GetFileName(), "Not enough memory to allocate field reference.");
}

Method *Module::ReadSingleMethod(ModuleReader &reader)
{
	FileMethodFlags methodFlags = reader.Read<FileMethodFlags>();

	String *name = FindString(reader.ReadToken());
	if (name == nullptr)
		throw ModuleLoadException(reader.GetFileName(), "Could not resolve string ID in MethodDef or FunctionDef name.");

	const uint32_t size = reader.ReadUInt32();
	if (size == 0)
		throw ModuleLoadException(reader.GetFileName(), "Method found without overloads.");
	
	const long posBefore = reader.GetPosition();
	const int32_t overloadCount = reader.ReadInt32();

	if (overloadCount == 0)
		throw ModuleLoadException(reader.GetFileName(), "Method found without overloads.");

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

	std::unique_ptr<Method> method(new Method(name, this, memberFlags));

	// Note: do not memset these to 0; MethodOverload has a default ctor now
	std::unique_ptr<MethodOverload[]> overloads(new MethodOverload[overloadCount]);

	for (int32_t i = 0; i < overloadCount; i++)
	{
		OverloadFlags flags = reader.Read<OverloadFlags>();

		MethodOverload *ov = overloads.get() + i;
		ov->group = method.get();

		// Parameter count & names
		uint16_t paramCount = reader.ReadUInt16();
		ov->paramCount = paramCount;
		ov->paramNames = new String*[paramCount];
		{
			// The +1 is to make sure that we always reserve space for the instance,
			// even if there isn't any
			RefSignatureBuilder refBuilder(paramCount + 1);

			for (int p = 0; p < paramCount; p++)
			{
				TokenId paramNameId = reader.ReadToken();
				ParamFlags paramFlags = reader.Read<ParamFlags>();
				ov->paramNames[p] = FindString(paramNameId);
				if (paramFlags == PF_BY_REF)
					refBuilder.SetParam(p + 1, true);
			}

			ov->refSignature = refBuilder.Commit(vm->GetRefSignaturePool());
		}

		// Flags
		ov->flags = (MethodFlags)0;
		if (methodFlags & FM_CTOR)
		{
			ov->flags |= MethodFlags::CTOR;
			method->flags |= MemberFlags::CTOR;
		}
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
			std::unique_ptr<MethodOverload::TryBlock[]> tries(nullptr);
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
				tries = std::unique_ptr<MethodOverload::TryBlock[]>(ReadTryBlocks(reader, tryCount));
			}

			ov->tryBlockCount = tryCount;
			ov->tryBlocks = tries.release();
		}

		// Body
		if (!(flags & OV_ABSTRACT))
		{
			if (flags & OV_NATIVE)
			{
				std::unique_ptr<char[]> entryPointName(reader.ReadCString());
				NativeMethod entryPoint = (NativeMethod)FindNativeEntryPoint(entryPointName.get());
				if (entryPoint == nullptr)
					throw ModuleLoadException(reader.GetFileName(), "Could not locate entry point of native method.");
				ov->nativeEntry = entryPoint;
				ov->flags |= MethodFlags::NATIVE;
			}
			else
			{
				uint32_t offset = reader.ReadUInt32(); // The offset of the first instruction in the method, relative to the method block
				uint32_t length = reader.ReadUInt32(); // The length of the body, in bytes

				const unsigned long posCurrent = reader.GetPosition(); // Resumption point

				// Read the method body
				reader.Seek(methodStart + offset, os::FILE_SEEK_START);
				std::unique_ptr<uint8_t[]> body(new uint8_t[length]);
				reader.Read(body.get(), length);

				// Return to previous position
				reader.Seek(posCurrent, os::FILE_SEEK_START);

				ov->length = length;
				ov->entry = body.release();
			}
		}
	}

	const long posAfter = reader.GetPosition();
	if (posBefore + (long)size != posAfter)
		throw ModuleLoadException(reader.GetFileName(), "The actual size of the overloads table did not match the expected size.");

	method->overloadCount = overloadCount;
	method->overloads = overloads.release();

	return method.release();
}

MethodOverload::TryBlock *Module::ReadTryBlocks(ModuleReader &reader, int32_t &tryCount)
{
	typedef MethodOverload::TryBlock::TryKind TryKind;
	std::unique_ptr<MethodOverload::TryBlock[]> output(nullptr);

	CHECKPOS_BEFORE();

	const int32_t length = reader.ReadInt32();

	output = std::unique_ptr<MethodOverload::TryBlock[]>(new MethodOverload::TryBlock[length]);
	MethodOverload::TryBlock *tries = output.get();

	for (int32_t i = 0; i < length; i++)
	{
		TryKind kind = reader.Read<TryKind>();
		uint32_t tryStart = reader.ReadUInt32();
		uint32_t tryEnd   = reader.ReadUInt32();

		MethodOverload::TryBlock *curTry = tries + i;
		*curTry = MethodOverload::TryBlock(kind, tryStart, tryEnd);

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
					std::unique_ptr<MethodOverload::CatchBlock[]> catches(new MethodOverload::CatchBlock[catchLength]);

					for (int32_t i = 0; i < catchLength; i++)
					{
						MethodOverload::CatchBlock *curCatch = catches.get() + i;

						curCatch->caughtTypeId = reader.ReadToken();
						// Try to resolve the type right away. If it fails, do it when the method
						// is initialized instead.
						if (FindType(curCatch->caughtTypeId))
							curCatch->caughtType = FindType(curCatch->caughtTypeId);
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

void Module::TryRegisterStandardType(Type *type, ModuleReader &reader)
{
	VM *vm = this->vm;
	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType stdType = std_type_names::Types[i];
		if (String_Equals(type->fullName, stdType.name))
		{
			if (vm->types.*(stdType.member) == nullptr)
			{
				vm->types.*(stdType.member) = type;
				if (stdType.initerFunction)
				{
					void *func = FindNativeEntryPoint(stdType.initerFunction);
					if (!func)
						throw ModuleLoadException(reader.GetFileName(), "Missing instance initializer for standard type in native library.");

					// Can't really switch here :(
					// Also because all initializer functions are of different types,
					// we can't really store a VM::IniterFunctions member in stdType.
					if (type == vm->types.List)
						vm->functions.initListInstance = (ListInitializer)func;
					else if (type == vm->types.Hash)
						vm->functions.initHashInstance = (HashInitializer)func;
					else if (type == vm->types.Type)
						vm->functions.initTypeToken = (TypeTokenInitializer)func;
				}
			}
			break;
		}
	}
}

void Module::AppendVersionString(PathName &path, ModuleVersion &version)
{
	typedef int32_t (ModuleVersion::*VersionField);
	static const int fieldCount = 4;
	static VersionField fields[] = {
		&ModuleVersion::major,
		&ModuleVersion::minor,
		&ModuleVersion::build,
		&ModuleVersion::revision,
	};

	for (int f = 0; f < fieldCount; f++)
	{
		if (f > 0)
			path.Append(_Path("."));

		int32_t value = version.*(fields[f]);

		// The max value of int32_t is 4,294,967,295, which is 10 characters.
		static const int charCount = 15;
		pathchar_t chars[charCount + 1];
		pathchar_t *pch = chars + charCount;
		// pch points to where we want \0
		*pch = _Path('\0');
		do
		{
			*--pch = (pathchar_t)(_Path('0') + value % 10);
			value /= 10;
		} while (value != 0);

		path.Append(pch);
	}
}

} // namespace ovum

// Paper thin API wrapper functions, whoo!

OVUM_API ModuleHandle FindModule(ThreadHandle thread, String *name, ModuleVersion *version)
{
	return thread->GetVM()->GetModulePool()->Get(name, version);
}

OVUM_API String *Module_GetName(ModuleHandle module)
{
	return module->GetName();
}

OVUM_API void Module_GetVersion(ModuleHandle module, ModuleVersion *version)
{
	*version = module->GetVersion();
}

OVUM_API String *Module_GetFileName(ThreadHandle thread, ModuleHandle module)
{
	return module->GetFileName().ToManagedString(thread);
}

OVUM_API bool Module_GetGlobalMember(ModuleHandle module, String *name, bool includeInternal, GlobalMember *result)
{
	ovum::ModuleMember member;
	if (module->FindMember(name, includeInternal, member))
	{
		result->flags = member.flags;
		result->name = member.name;
		switch (member.flags & ModuleMemberFlags::KIND)
		{
		case ModuleMemberFlags::TYPE:
			result->type = member.type;
			break;
		case ModuleMemberFlags::FUNCTION:
			result->function = member.function;
			break;
		case ModuleMemberFlags::CONSTANT:
			result->constant = member.constant;
			break;
		}
		return true;
	}
	return false;
}

OVUM_API int32_t Module_GetGlobalMemberCount(ModuleHandle module)
{
	return module->GetMemberCount();
}

OVUM_API bool Module_GetGlobalMemberByIndex(ModuleHandle module, int32_t index, GlobalMember *result)
{
	ovum::ModuleMember member;
	if (module->GetMemberByIndex(index, member))
	{
		result->flags = member.flags;
		result->name = member.name;
		switch (member.flags & ModuleMemberFlags::KIND)
		{
		case ModuleMemberFlags::TYPE:
			result->type = member.type;
			break;
		case ModuleMemberFlags::FUNCTION:
			result->function = member.function;
			break;
		case ModuleMemberFlags::CONSTANT:
			result->constant = member.constant;
			break;
		}
		return true;
	}
	return false;
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

OVUM_API ModuleHandle Module_FindDependency(ModuleHandle module, String *name)
{
	return module->FindModuleRef(name);
}