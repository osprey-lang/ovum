#include "modulereader.h"
#include "module.h"
#include "modulefacts.h"
#include "modulepool.h"
#include "../gc/gc.h"
#include "../object/type.h"
#include "../object/field.h"
#include "../object/method.h"
#include "../object/property.h"
#include "../ee/refsignature.h"
#include "../res/staticstrings.h"

// Strictly for convenience
namespace mf = ovum::module_file;

namespace ovum
{

ModuleFile::ModuleFile() :
	data(nullptr),
	file(),
	fileName(256)
{ }

ModuleFile::~ModuleFile()
{
	if (data != nullptr)
		os::UnmapView(data);

	os::CloseMemoryMappedFile(&file);
}

void ModuleFile::Open(const pathchar_t *fileName)
{

	this->fileName.Clear();
	this->fileName.Append(fileName);

	os::FileStatus r = os::OpenMemoryMappedFile(
		fileName,
		os::FILE_OPEN,
		os::MMF_OPEN_READ,
		os::FILE_SHARE_READ,
		&this->file
	);
	if (r != os::FILE_OK)
		HandleFileOpenError(r);

	data = os::MapView(&file, os::MMF_VIEW_READ, 0, 0);
}

void ModuleFile::HandleFileOpenError(os::FileStatus error)
{
	const char *message;
	switch (error)
	{
	case os::FILE_NOT_FOUND:     message = "The file could not be found.";   break;
	case os::FILE_ACCESS_DENIED: message = "Access to the file was denied."; break;
	default:                     message = "Unspecified I/O error.";         break;
	}
	throw ModuleIOException(message);
}

ModuleReader::ModuleReader(VM *owner, PartiallyOpenedModulesList &partiallyOpenedModules) :
	file(),
	vm(owner),
	unresolvedConstants(),
	partiallyOpenedModules(partiallyOpenedModules)
{ }

ModuleReader::~ModuleReader()
{ }

void ModuleReader::Open(const pathchar_t *fileName)
{
	file.Open(fileName);
}
void ModuleReader::Open(const PathName &fileName)
{
	Open(fileName.GetDataPointer());
}

Box<Module> ModuleReader::ReadModule()
{
	const mf::ModuleHeader *header = file.Read<mf::ModuleHeader>(0);
	VerifyHeader(header);

	ModuleParams params;
	params.name = ReadString(header->name);
	params.version = ReadVersion(header->version);
	params.globalMemberCount = static_cast<size_t>(
		header->typeCount +
		header->functionCount +
		header->constantCount
	);

	Box<Module> output(new Module(vm, GetFileName(), params));
	// We have to add the module to the list of partially opened modules, so that
	// we can detect circular references.
	partiallyOpenedModules.Add(output.get());

	// Load the native library first, if there is one.
	if (!header->nativeLib.IsNull())
		ReadNativeLibrary(output.get(), header->nativeLib);

	// The first true managed thing we read is the string table, since pretty much
	// every member refers to it.
	ReadStringTable(output.get(), file.Deref(header->strings));

	// Definitions generally depend on references, so we'll read the references next.
	ReadReferences(output.get(), file.Deref(header->references));

	// And finally definitions!
	ReadDefinitions(output.get(), header);

	VerifyAnnotations(header->annotations);

	// Now that we're done reading all the members, we only need to locate the main
	// method (if there is one), and call the native library's initialization (if
	// applicable).
	if (header->mainMethod != 0)
		output->mainMethod = GetMainMethod(output.get(), header->mainMethod);

	if (os::LibraryHandleIsValid(&output->nativeLib))
	{
		mf::NativeModuleMain nativeMain = 
			(mf::NativeModuleMain)output->FindNativeEntryPoint(mf::NativeModuleIniterName);
		if (nativeMain != nullptr)
			nativeMain(output.get());
	}

	partiallyOpenedModules.Remove(output.get());
	return std::move(output);
}

String *ModuleReader::ReadString(mf::Rva<mf::WideString> rva)
{
	const mf::WideString *str = file.Deref(rva);

	String *string = GetGC()->ConstructModuleString(
		nullptr,
		(size_t)str->length,
		str->chars.Get()
	);
	// If a string with this value is already interned, we get that string instead.
	// If we have that string, GC::InternString does nothing; if we don't, we have
	// a brand new string and interning it actually interns it.
	string = GetGC()->InternString(nullptr, string);

	return string;
}

String *ModuleReader::ResolveString(Module *module, Token token)
{
	String *str = module->FindString(token);

	if (str == nullptr)
		ModuleLoadError("Unresolved string token.");

	return str;
}

void ModuleReader::ReadNativeLibrary(Module *module, mf::Rva<mf::WideString> libraryNameRva)
{
	String *libraryName = ReadString(libraryNameRva);

	// Native libraries are always loaded from the module's folder.
	module->LoadNativeLibrary(libraryName, file.GetFileName());
}

Method *ModuleReader::GetMainMethod(Module *module, Token token)
{
	Method *method = module->FindMethod(token);
	if (method == nullptr)
		ModuleLoadError("Unresolved main method.");
	if (!method->IsStatic())
		ModuleLoadError("Main method must be a static method.");
	return method;
}

void ModuleReader::ReadStringTable(Module *module, const mf::StringTableHeader *header)
{
	size_t count = (size_t)header->length;
	module->strings.Init(count);

	const mf::Rva<mf::WideString> *strings = header->strings.Get();
	for (size_t i = 0; i < count; i++)
	{
		String *str = ReadString(strings[i]);
		module->strings.Add(str);
	}
}

void ModuleReader::ReadReferences(Module *module, const mf::RefTableHeader *header)
{
	ReadModuleRefs(module, header);

	// Field and method refs depend on type refs, so read type first.
	ReadTypeRefs(module, header);

	ReadFieldRefs(module, header);
	ReadMethodRefs(module, header);

	ReadFunctionRefs(module, header);
}

void ModuleReader::ReadModuleRefs(Module *module, const mf::RefTableHeader *header)
{
	if (header->moduleRefCount == 0)
		return;

	MemberTable<Module*> &moduleRefs = module->moduleRefs;

	size_t count = (size_t)header->moduleRefCount;
	moduleRefs.Init(count);

	const mf::ModuleRef *refs = file.Deref(header->moduleRefs);
	for (size_t i = 0; i < count; i++)
	{
		const mf::ModuleRef *ref = refs + i;

		// TODO: Implement support for version constraints. For now, the
		// required version is treated as an absolute.
		String *name = ResolveString(module, ref->name);
		ModuleVersion version = ReadVersion(ref->version);

		// If the module is partially opened, we must be dealing with a
		// circular reference, whether direct or indirect.
		if (partiallyOpenedModules.Contains(name, &version))
			ModuleLoadError("Circular dependency detected.");

		Module *importedModule = Module::OpenByName(vm, name, &version, partiallyOpenedModules);
		if (importedModule->version != version)
			ModuleLoadError("Dependent module has the wrong version.");

		moduleRefs.Add(importedModule);
	}
}

void ModuleReader::ReadTypeRefs(Module *module, const mf::RefTableHeader *header)
{
	if (header->typeRefCount == 0)
		return;

	MemberTable<Type*> &typeRefs = module->typeRefs;

	size_t count = (size_t)header->typeRefCount;
	typeRefs.Init(count);

	const mf::TypeRef *refs = file.Deref(header->typeRefs);
	for (size_t i = 0; i < count; i++)
	{
		const mf::TypeRef *ref = refs + i;

		Module *declModule = module->FindModuleRef(ref->declModule);
		if (declModule == nullptr)
			ModuleLoadError("Unresolved ModuleRef token in TypeRef.");

		String *typeName = ResolveString(module, ref->name);

		Type *type = declModule->FindType(typeName, false);
		if (type == nullptr)
			ModuleLoadError("Cannot find a matching type for TypeRef.");

		typeRefs.Add(type);
	}
}

void ModuleReader::ReadFieldRefs(Module *module, const mf::RefTableHeader *header)
{
	if (header->fieldRefCount == 0)
		return;

	MemberTable<Field*> &fieldRefs = module->fieldRefs;

	size_t count = (size_t)header->fieldRefCount;
	fieldRefs.Init(count);

	const mf::FieldRef *refs = file.Deref(header->fieldRefs);
	for (size_t i = 0; i < count; i++)
	{
		const mf::FieldRef *ref = refs + i;

		Type *declType = module->FindType(ref->declType);
		if (declType == nullptr)
			ModuleLoadError("Unresolved TypeRef token in FieldRef.");

		String *fieldName = ResolveString(module, ref->name);

		Member *member = declType->GetMember(fieldName);
		if (member == nullptr)
			ModuleLoadError("No matching member for FieldRef.");
		if (!member->IsField())
			ModuleLoadError("FieldRef does not resolve to a field.");

		fieldRefs.Add(static_cast<Field*>(member));
	}
}

void ModuleReader::ReadMethodRefs(Module *module, const mf::RefTableHeader *header)
{
	if (header->methodRefCount == 0)
		return;

	MemberTable<Method*> &methodRefs = module->methodRefs;

	size_t count = (size_t)header->methodRefCount;
	methodRefs.Init(count);

	const mf::MethodRef *refs = file.Deref(header->methodRefs);
	for (size_t i = 0; i < count; i++)
	{
		const mf::MethodRef *ref = refs + i;

		Type *declType = module->FindType(ref->declType);
		if (declType == nullptr)
			ModuleLoadError("Unresolved TypeRef token in MethodRef.");

		String *methodName = ResolveString(module, ref->name);

		Member *member = declType->GetMember(methodName);
		if (member == nullptr)
			ModuleLoadError("No matching member for MethodRef.");
		if (!member->IsMethod())
			ModuleLoadError("MethodRef does not resolve to a method.");

		methodRefs.Add(static_cast<Method*>(member));
	}
}

void ModuleReader::ReadFunctionRefs(Module *module, const mf::RefTableHeader *header)
{
	if (header->functionRefCount == 0)
		return;

	MemberTable<Method*> &functionRefs = module->functionRefs;

	size_t count = (size_t)header->functionRefCount;
	functionRefs.Init(count);

	const mf::FunctionRef *refs = file.Deref(header->functionRefs);
	for (size_t i = 0; i < count; i++)
	{
		const mf::FunctionRef *ref = refs + i;

		Module *declModule = module->FindModuleRef(ref->declModule);
		if (declModule == nullptr)
			ModuleLoadError("Unresolved ModuleRef token in FunctionRef.");

		String *functionName = ResolveString(module, ref->name);

		Method *function = declModule->FindGlobalFunction(functionName, false);
		if (function == nullptr)
			ModuleLoadError("No matching member for FunctionRef.");

		functionRefs.Add(function);
	}
}

void ModuleReader::ReadDefinitions(Module *module, const mf::ModuleHeader *header)
{
	ReadTypeDefs(module, header);

	// Now that all types have been read, we have enough information to
	// resolve any constant field values whose types we couldn't find on
	// the first pass.
	ResolveRemainingConstants(module);

	ReadFunctionDefs(module, header);

	ReadConstantDefs(module, header);
}

void ModuleReader::ReadTypeDefs(Module *module, const mf::ModuleHeader *header)
{
	if (header->typeCount == 0)
		return;

	auto &typeDefs = module->types;

	size_t count = (size_t)header->typeCount;
	typeDefs.Init(count);
	module->fields.Init(header->fieldCount);
	module->methods.Init(header->methodCount);

	const mf::TypeDef *defs = file.Deref(header->types);
	for (size_t i = 0; i < count; i++)
	{
		const mf::TypeDef *def = defs + i;

		Box<Type> type = ReadSingleTypeDef(module, header, def);

		GlobalMember member = GlobalMember::FromType(type.get());
		if (!module->members.Add(type->fullName, member))
			ModuleLoadError("Duplicate global member name.");

		module->TryRegisterStandardType(type.get());

		typeDefs.Add(std::move(type));
	}
}

Box<Type> ModuleReader::ReadSingleTypeDef(Module *module, const mf::ModuleHeader *header, const mf::TypeDef *def)
{
	// Try to resolve as much as possible before constructing the Type.
	String *name = ResolveString(module, def->name);

	Type *baseType = GetBaseType(module, def->baseType);
	Type *sharedType = GetSharedType(module, def->sharedType);

	size_t memberCount = static_cast<size_t>(
		def->fieldCount +
		def->methodCount +
		def->propertyCount
	);
	Box<Type> type(new Type(module, memberCount));
	// TypeFlags is compatible with module_file::TypeFlags by design.
	type->flags = static_cast<TypeFlags>(def->flags);
	type->fullName = name;

	type->baseType = baseType;
	type->sharedType = sharedType;

	type->fieldsOffset = baseType != nullptr ? baseType->GetTotalSize() : 0;

	// Type members
	ReadFieldDefs(
		module,
		type.get(),
		header->fields.address,
		(size_t)def->fieldCount,
		def->firstField
	);
	ReadMethodDefs(
		module,
		type.get(),
		header->methods.address,
		(size_t)def->methodCount,
		def->firstMethod
	);
	ReadPropertyDefs(
		module,
		type.get(),
		(size_t)def->propertyCount,
		def->properties
	);
	ReadOperatorDefs(
		module,
		type.get(),
		(size_t)def->operatorCount,
		def->operators
	);

	VerifyAnnotations(def->annotations);

	// Lift inherited operators to the newly declared type
	type->InitOperators();

	type->instanceCtor = FindInstanceConstructor(type.get());

	if (!def->initer.IsNull())
		RunTypeIniter(module, type.get(), def->initer);

	// If the base type has a finalizer, we must ensure it is called
	// when this type is destroyed too.
	if (baseType && baseType->HasFinalizer())
		type->flags |= TypeFlags::HAS_FINALIZER;

	return std::move(type);
}

Type *ModuleReader::GetBaseType(Module *module, Token baseTypeToken)
{
	if (baseTypeToken == 0)
		return nullptr;

	Type *result = module->FindType(baseTypeToken);
	if (result == nullptr)
		ModuleLoadError("Unresolved base type in TypeDef.");
	return result;
}

Type *ModuleReader::GetSharedType(Module *module, Token sharedTypeToken)
{
	if (sharedTypeToken == 0)
		return nullptr;

	if ((sharedTypeToken & mf::TOKEN_KIND_MASK) != mf::TOKEN_TYPEDEF)
		ModuleLoadError("The sharedType of a TypeDef must be a TypeDef token.");

	Type *result = module->FindType(sharedTypeToken);
	if (result == nullptr)
		ModuleLoadError("Unresolved shared type in TypeDef.");
	return result;
}

Method *ModuleReader::FindInstanceConstructor(Type *type)
{
	Member *member = type->GetMember(vm->GetStrings()->members.new_);

	// The instance constructor must be an instance method.
	if (member == nullptr || member->IsStatic() || !member->IsMethod())
		return nullptr;

	return static_cast<Method*>(member);
}

void ModuleReader::RunTypeIniter(Module *module, Type *type, mf::Rva<mf::ByteString> initerRva)
{
	const mf::ByteString *initerName = file.Deref(initerRva);

	TypeInitializer initerFunc = (TypeInitializer)module->FindNativeEntryPoint(initerName->chars.Get());
	if (initerFunc == nullptr)
		ModuleLoadError("Unresolved type initializer entry point name.");

	int r = initerFunc(type);
	if (r != OVUM_SUCCESS)
		ModuleLoadError("An error occurred when executing a type initializer.");
}

void ModuleReader::ReadFieldDefs(Module *module, Type *type, uint32_t fieldsBase, size_t count, Token firstField)
{
	if (count == 0)
		return;

	auto &fields = module->fields;

	uint32_t tokenIndex = (firstField & mf::TOKEN_INDEX_MASK) - 1;
	const mf::FieldDef *defs = file.Read<mf::FieldDef>(
		fieldsBase + sizeof(mf::FieldDef) * tokenIndex
	);
	for (size_t i = 0; i < count; i++)
	{
		const mf::FieldDef *def = defs + i;

		MemberFlags flags = GetMemberFlags(def->flags);
		String *name = ResolveString(module, def->name);

		Box<Field> field(new Field(name, type, flags));

		if ((def->flags & mf::FIELD_HAS_VALUE) == mf::FIELD_HAS_VALUE)
			ReadFieldConstantValue(module, field.get(), file.Deref(def->value), true);

		if (field->IsStatic())
		{
			// Initialized only on demand.
			field->staticValue = nullptr;
		}
		else
		{
			field->offset = type->GetTotalSize();
			type->fieldCount++;
			type->size += sizeof(Value);
		}

		VerifyAnnotations(def->annotations);

		if (!type->members.Add(name, field.get()))
			ModuleLoadError("Duplicate member name in type.");
		fields.Add(std::move(field));
	}
}

void ModuleReader::ReadFieldConstantValue(Module *module, Field *field, const mf::ConstantValue *value, bool allowUnresolvedType)
{
	Value fieldValue;
	if (!ReadConstantValue(module, value, fieldValue))
	{
		if (allowUnresolvedType)
			// Let's figure out the type when we're done loading all the types instead.
			AddUnresolvedConstant(module, field, value);
		else
			ModuleLoadError("Unresolved type in constant field value.");
		return;
	}

	field->staticValue = GetGC()->AddStaticReference(nullptr, &fieldValue);
	if (!field->staticValue)
		ModuleLoadError("Unable to allocate memory for constant field.");
}

void ModuleReader::AddUnresolvedConstant(Module *module, Field *field, const mf::ConstantValue *value)
{
	UnresolvedConstant info;
	info.field = field;
	info.value = value;
	unresolvedConstants.push_back(info);
}

void ModuleReader::ReadMethodDefs(Module *module, Type *type, uint32_t methodsBase, size_t count, Token firstMethod)
{
	if (count == 0)
		return;

	auto &methods = module->methods;

	uint32_t tokenIndex = (firstMethod & mf::TOKEN_INDEX_MASK) - 1;
	const mf::MethodDef *defs = file.Read<mf::MethodDef>(
		methodsBase + sizeof(mf::FieldDef) * tokenIndex
	);
	for (size_t i = 0; i < count; i++)
	{
		const mf::MethodDef *def = defs + i;

		Box<Method> method = ReadSingleMethodDef(module, def);

		if (!type->members.Add(method->name, method.get()))
			ModuleLoadError("Duplicate member name in type.");
		method->SetDeclType(type);

		method->baseMethod = FindBaseMethod(method.get(), type);

		methods.Add(std::move(method));
	}
}

Method *ModuleReader::FindBaseMethod(Method *method, Type *declType)
{
	// If this method is not private and the declaring type has
	// a base type, we must search all base types for a public or
	// protected method of the same name. This will become the
	// baseMethod of 'method', which we need in case this method
	// has overloaded an inherited method group.
	//
	// We can skip this step entirely for some special methods:
	//   - '.new', '.init': these methods are never considered to
	//     overload their base type methods of the same name.
	//     These methods have the CTOR flag, so we don't need to
	//     compare by name here.
	//   - '.iter': this method can only ever have a single overload
	//     which is always public and parameterless.
	if (declType->baseType == nullptr ||
		method->IsPrivate() ||
		method->IsCtor() ||
		String_Equals(method->name, vm->GetStrings()->members.iter_))
		return nullptr;

	Type *t = declType->baseType;
	do
	{
		Member *member = t->GetMember(method->name);
		if (member != nullptr)
		{
			// The two members are considered matching if:
			//   1. They have the same accessibility. Here we must obey
			//      an extra rule: if both methods are internal, they
			//      also need to belong to the same module.
			//   2. They are both either static or instance members.
			//   3. They are both methods.

			// Generally we avoid using MemberFlags outside Member, but
			// for the sake of simplicity and speediness, we can use them
			// a little bit here...
			static const MemberFlags MATCHING_FLAGS =
				MemberFlags::ACCESSIBILITY |
				MemberFlags::KIND_MASK |
				MemberFlags::INSTANCE;

			if ((method->flags & MATCHING_FLAGS) != (member->flags & MATCHING_FLAGS))
				continue;
			if (method->IsInternal() && method->declModule != member->declModule)
				continue;
			// Since the KIND_MASK matches, we know the other member
			// must be method.
			return static_cast<Method*>(member);
		}
	} while (t = t->baseType);

	return nullptr;
}

void ModuleReader::ReadPropertyDefs(Module *module, Type *type, size_t count, mf::Rva<mf::PropertyDef[]> properties)
{
	if (count == 0)
		return;

	const mf::PropertyDef *defs = file.Deref(properties);
	for (size_t i = 0; i < count; i++)
	{
		const mf::PropertyDef *def = defs + i;

		String *name = ResolveString(module, def->name);

		// The getter and setter have to have certain common flags. We store
		// the flags of the first encountered accessor here. These flags are
		// also passed to the Property constructor.
		MemberFlags flags = MemberFlags::NONE;

		Method *getter = ReadPropertyAccessor(module, type, def->getter, flags);
		Method *setter = ReadPropertyAccessor(module, type, def->setter, flags);

		if (getter == nullptr && setter == nullptr)
			ModuleLoadError("PropertyDef must have at least one accessor.");

		Box<Property> property(new Property(name, type, flags));
		property->getter = getter;
		property->setter = setter;

		// ResolveOverload may return null in these cases, which is fine: if
		// it's an indexer property, there may not be any meaningful default
		// accessors.
		if (getter)
			property->defaultGetter = getter->ResolveOverload(0);
		if (setter)
			property->defaultSetter = setter->ResolveOverload(1);

		if (!type->members.Add(name, property.get()))
			ModuleLoadError("Duplicate member name in type.");
		property.release(); // The type owns it now
	}
}

Method *ModuleReader::ReadPropertyAccessor(Module *module, Type *type, Token token, MemberFlags &flags)
{
	if (token == 0)
		return nullptr;

	if ((token & mf::TOKEN_KIND_MASK) != mf::TOKEN_METHODDEF)
		ModuleLoadError("Property accessor must be a MethodDef token.");

	Method *accessor = module->FindMethod(token);
	if (accessor == nullptr)
		ModuleLoadError("Unresolved MethodDef token in property accessor.");
	if (accessor->declType != type)
		ModuleLoadError("Property accessor must belong to the property's declaring type.");

	static const MemberFlags INTERESTING_FLAGS =
		MemberFlags::ACCESSIBILITY |
		MemberFlags::INSTANCE |
		MemberFlags::IMPL;

	MemberFlags accessorFlags = accessor->flags & INTERESTING_FLAGS;

	if (flags == MemberFlags::NONE)
		flags = accessorFlags;
	else if (flags != accessorFlags)
		ModuleLoadError("Property accessors must have matching accessibility, instance and impl flags.");

	return accessor;
}

void ModuleReader::ReadOperatorDefs(Module *module, Type *type, size_t count, mf::Rva<mf::OperatorDef[]> operators)
{
	if (count == 0)
		return;

	const mf::OperatorDef *defs = file.Deref(operators);
	for (size_t i = 0; i < count; i++)
	{
		const mf::OperatorDef *def = defs + i;

		if ((def->method & mf::TOKEN_KIND_MASK) != mf::TOKEN_METHODDEF)
			ModuleLoadError("Operator method must be a MethodDef token.");

		Method *method = module->FindMethod(def->method);
		if (method == nullptr)
			ModuleLoadError("Unresolved method in OperatorDef.");
		if (method->declType != type)
			ModuleLoadError("Operator method must belong to the operator's declaring type.");
		if (!method->IsStatic())
			ModuleLoadError("Operator method must be static.");
		if (type->operators[(int)def->op] != nullptr)
			ModuleLoadError("Duplicate operator declaration.");

		MethodOverload *overload = method->ResolveOverload(Arity(static_cast<Operator>(def->op)));
		if (overload == nullptr)
			ModuleLoadError("Operator must contain an overload matching the operator's arity.");

		type->operators[(int)def->op] = overload;
	}
}

void ModuleReader::ResolveRemainingConstants(Module *module)
{
	for (auto const &value : unresolvedConstants)
	{
		ReadFieldConstantValue(module, value.field, value.value, false);
	}

	unresolvedConstants.clear();
}

void ModuleReader::ReadFunctionDefs(Module *module, const mf::ModuleHeader *header)
{
	if (header->functionCount == 0)
		return;

	auto &functions = module->functions;

	size_t count = (size_t)header->functionCount;
	functions.Init(count);

	const mf::MethodDef *defs = file.Deref(header->functions);
	for (size_t i = 0; i < count; i++)
	{
		const mf::MethodDef *def = defs + i;

		Box<Method> function = ReadSingleMethodDef(module, def);
		function->SetDeclType(nullptr);

		GlobalMember member = GlobalMember::FromFunction(function.get());
		if (!module->members.Add(function->name, member))
			ModuleLoadError("Duplicate global member name.");

		functions.Add(std::move(function));
	}
}

void ModuleReader::ReadConstantDefs(Module *module, const mf::ModuleHeader *header)
{
	if (header->constantCount == 0)
		return;

	size_t count = (size_t)header->constantCount;
	const mf::ConstantDef *defs = file.Deref(header->constants);
	for (size_t i = 0; i < count; i++)
	{
		const mf::ConstantDef *def = defs + i;

		String *name = ResolveString(module, def->name);

		Value value;
		if (!ReadConstantValue(module, file.Deref(def->value), value))
			ModuleLoadError("Unresolved type in ConstantDef.");

		VerifyAnnotations(def->annotations);

		GlobalMember member = GlobalMember::FromConstant(
			name,
			&value,
			(def->flags & mf::CONSTANT_INTERNAL) == mf::CONSTANT_INTERNAL
		);
		if (!module->members.Add(name, member))
			ModuleLoadError("Duplicate global member name.");
	}
}

bool ModuleReader::ReadConstantValue(Module *module, const mf::ConstantValue *value, Value &result)
{
	// Null is represented by type = 0
	if (value->type == 0)
	{
		result.type = nullptr;
		result.v.uinteger = 0;
		return true;
	}

	Type *type = module->FindType(value->type);
	if (type == nullptr)
		// Sometimes recoverable error: undefined type.
		return false;

	if (!type->IsPrimitive() && type != vm->types.String)
		ModuleLoadError("Constant type in FieldDef must be primitive or aves.String.");

	result.type = type;
	if (type == vm->types.String)
	{
		String *str = ResolveString(module, value->v.stringValue);
		result.v.string = str;
	}
	else
	{
		result.v.uinteger = value->v.uintValue;
	}

	return true;
}

Box<Method> ModuleReader::ReadSingleMethodDef(Module *module, const mf::MethodDef *def)
{
	String *name = ResolveString(module, def->name);
	MemberFlags flags = GetMemberFlags(def->flags);

	size_t overloadCount = (size_t)def->overloadCount;
	if (overloadCount == 0)
		ModuleLoadError("Method must have at least one overload.");

	Box<Method> method(new Method(name, module, flags));

	// Note: this is not an array of pointers!
	Box<MethodOverload[]> overloads(new MethodOverload[overloadCount]);

	const mf::OverloadDef *overloadDefs = file.Deref(def->overloads);
	for (size_t i = 0; i < overloadCount; i++)
	{
		const mf::OverloadDef *overloadDef = overloadDefs + i;

		MethodOverload *overload = overloads.get() + i;
		overload->group = method.get();
		overload->flags = GetOverloadFlags(overloadDef->flags);

		// Some flags need to be copied from the Method
		if (!method->IsStatic())
		{
			overload->flags |= OverloadFlags::INSTANCE;
			overload->instanceCount = 1;
		}
		if (method->IsCtor())
			overload->flags |= OverloadFlags::CTOR;

		ReadParameters(
			module,
			overload,
			(size_t)overloadDef->paramCount,
			overloadDef->params
		);

		VerifyAnnotations(overloadDef->annotations);

		// If the overload isn't abstract, it has a body of some kind,
		// so we have to read it.
		if (!overload->IsAbstract())
			ReadMethodBody(module, overload, overloadDef);
	}

	method->overloadCount = overloadCount;
	method->overloads = overloads.release();

	return std::move(method);
}

void ModuleReader::ReadParameters(Module *module, MethodOverload *overload, size_t count, mf::Rva<mf::Parameter[]> rva)
{
	if (count > 0)
	{
		Box<String*[]> paramNames(new String*[count]);
		// Always reserve space for the instance, even if there isn't any.
		RefSignatureBuilder refBuilder(static_cast<ovlocals_t>(count + 1));
		ovlocals_t optionalCount = 0;

		const mf::Parameter *defs = file.Deref(rva);
		for (size_t i = 0; i < count; i++)
		{
			const mf::Parameter *def = defs + i;

			paramNames[i] = ResolveString(module, def->name);

			if ((def->flags & mf::PARAM_BY_REF) == mf::PARAM_BY_REF)
				refBuilder.SetParam(i + 1, true);

			if ((def->flags & mf::PARAM_OPTIONAL) == mf::PARAM_OPTIONAL)
				optionalCount++;
			else if (optionalCount > 0)
				ModuleLoadError("Required parameter cannot follow optional parameter.");
		}

		overload->paramCount = (ovlocals_t)count;
		overload->paramNames = paramNames.release();
		overload->optionalParamCount = optionalCount;
		overload->refSignature = refBuilder.Commit(vm->GetRefSignaturePool());
	}
	else
	{
		overload->paramCount = 0;
		overload->paramNames = nullptr;
		overload->optionalParamCount = 0;
		overload->refSignature = 0;
	}
}

void ModuleReader::ReadMethodBody(Module *module, MethodOverload *overload, const mf::OverloadDef *def)
{
	if (overload->IsNative())
		ReadNativeMethodBody(module, overload, file.Deref(def->h.nativeHeader));
	else if (overload->HasShortHeader())
		ReadShortMethodBody(module, overload, file.Deref(def->h.shortHeader));
	else
		ReadLongMethodBody(module, overload, file.Deref(def->h.longHeader));
}

void ModuleReader::ReadNativeMethodBody(Module *module, MethodOverload *overload, const mf::NativeMethodHeader *header)
{
	const char *entryPointName = header->entryPointName.chars.Get();
	NativeMethod entryPoint = (NativeMethod)module->FindNativeEntryPoint(entryPointName);

	if (entryPointName == nullptr)
		ModuleLoadError("Unresolved native method entry point name.");

	overload->nativeEntry = entryPoint;
	overload->locals = header->localCount;
}

void ModuleReader::ReadShortMethodBody(Module *module, MethodOverload *overload, const mf::MethodBody *header)
{
	// Short header implies certain defaults:
	overload->tryBlockCount = 0;
	overload->tryBlocks = nullptr;
	overload->maxStack = 8;

	ReadBytecodeBody(module, overload, header);
}

void ModuleReader::ReadLongMethodBody(Module *module, MethodOverload *overload, const mf::MethodHeader *header)
{
	overload->maxStack = header->maxStack;
	overload->locals = header->localCount;

	ReadTryBlocks(module, overload, (size_t)header->tryBlockCount, header->tryBlocks);

	ReadBytecodeBody(module, overload, &header->body);
}

void ModuleReader::ReadTryBlocks(Module *module, MethodOverload *overload, size_t count, mf::Rva<mf::TryBlock[]> rva)
{
	if (count == 0)
	{
		overload->tryBlockCount = 0;
		overload->tryBlocks = nullptr;
		return;
	}

	Box<TryBlock[]> tryBlocks(new TryBlock[count]);

	const module_file::TryBlock *defs = file.Deref(rva);
	for (size_t i = 0; i < count; i++)
	{
		const module_file::TryBlock *def = defs + i;

		TryBlock *tryBlock = tryBlocks.get() + i;

		switch (def->kind)
		{
		case mf::TRY_CATCH:
			{
				*tryBlock = TryBlock(TryKind::CATCH, def->tryStart, def->tryEnd);
				ReadCatchClauses(module, tryBlock, def->m.catchClauses);
			}
			break;
		case mf::TRY_FINALLY:
			{
				*tryBlock = TryBlock(TryKind::FINALLY, def->tryStart, def->tryEnd);
				tryBlock->finallyBlock.finallyStart = def->m.finallyClause.finallyStart;
				tryBlock->finallyBlock.finallyEnd = def->m.finallyClause.finallyEnd;
			}
			break;
		case mf::TRY_FAULT:
			{
				*tryBlock = TryBlock(TryKind::FAULT, def->tryStart, def->tryEnd);
				tryBlock->finallyBlock.finallyStart = def->m.finallyClause.finallyStart;
				tryBlock->finallyBlock.finallyEnd = def->m.finallyClause.finallyEnd;
			}
			break;
		default:
			ModuleLoadError("Invalid try block kind.");
			break;
		}
	}

	overload->tryBlockCount = count;
	overload->tryBlocks = tryBlocks.release();
}

void ModuleReader::ReadCatchClauses(Module *module, TryBlock *tryBlock, const mf::CatchClauses &catchClauses)
{
	if (catchClauses.count == 0)
		ModuleLoadError("A try-catch block must have at least one catch clause.");

	size_t count = (size_t)catchClauses.count;
	Box<CatchBlock[]> catchBlocks(new CatchBlock[count]);

	const mf::CatchClause *clauses = file.Deref(catchClauses.clauses);
	for (size_t i = 0; i < count; i++)
	{
		const mf::CatchClause *clause = clauses + i;

		CatchBlock *block = catchBlocks.get() + i;
		block->caughtTypeId = clause->caughtType;
		// Try to resolve the caught type right away. If we don't find anything,
		// it might be because the type hasn't been read yet, so do it later when
		// the method is initialized.
		block->caughtType = module->FindType(clause->caughtType);
		block->catchStart = clause->catchStart;
		block->catchEnd = clause->catchEnd;
	}

	tryBlock->catches.count = count;
	tryBlock->catches.blocks = catchBlocks.release();
}

void ModuleReader::ReadBytecodeBody(Module *module, MethodOverload *overload, const mf::MethodBody *body)
{
	Box<uint8_t[]> bodyBytes(new uint8_t[body->size]);
	CopyMemoryT(bodyBytes.get(), body->data.Get(), static_cast<size_t>(body->size));

	overload->length = (size_t)body->size;
	overload->entry = bodyBytes.release();
}

MemberFlags ModuleReader::GetMemberFlags(mf::FieldFlags flags)
{
	// Field accessibility is compatible with MemberFlags by design.
	MemberFlags result = static_cast<MemberFlags>(flags) & MemberFlags::ACCESSIBILITY;

	if ((flags & mf::FIELD_INSTANCE) == mf::FIELD_INSTANCE)
		result |= MemberFlags::INSTANCE;
	if ((flags & mf::FIELD_IMPL) == mf::FIELD_IMPL)
		result |= MemberFlags::IMPL;

	return result;
}

MemberFlags ModuleReader::GetMemberFlags(mf::MethodFlags flags)
{
	// Method accessibility is compatible with MemberFlags by design.
	MemberFlags result = static_cast<MemberFlags>(flags) & MemberFlags::ACCESSIBILITY;

	if ((flags & mf::METHOD_INSTANCE) == mf::METHOD_INSTANCE)
		result |= MemberFlags::INSTANCE;
	if ((flags & mf::METHOD_IMPL) == mf::METHOD_IMPL)
		result |= MemberFlags::IMPL;
	if ((flags & mf::METHOD_CTOR) == mf::METHOD_CTOR)
		result |= MemberFlags::CTOR;

	return result;
}

OverloadFlags ModuleReader::GetOverloadFlags(mf::OverloadFlags flags)
{
	// ovum::OverloadFlags is fully compatible with mf::OverloadFlags by design.
	return static_cast<OverloadFlags>(flags);
}

ModuleVersion ModuleReader::ReadVersion(const mf::ModuleVersion &version)
{
	ModuleVersion result;
	result.major = version.major;
	result.minor = version.minor;
	result.patch = version.patch;
	return result;
}

void ModuleReader::VerifyHeader(const mf::ModuleHeader *header)
{
	if (header->magic.number != mf::ExpectedMagicNumber.number)
		ModuleLoadError("Invalid magic number in module file.");

	if (header->formatVersion < mf::MinFileFormatVersion ||
		header->formatVersion > mf::MaxFileFormatVersion)
		ModuleLoadError("Unsupported module file format version.");
}

void ModuleReader::VerifyAnnotations(mf::Rva<mf::Annotations> rva)
{
	if (!rva.IsNull())
		ModuleLoadError("Annotations are not yet supported.");
}

OVUM_NOINLINE void ModuleReader::ModuleLoadError(const char *message)
{
	throw ModuleLoadException(GetFileName(), message);
}

} // namespace ovum
