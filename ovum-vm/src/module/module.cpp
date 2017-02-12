#include "module.h"
#include "modulepool.h"
#include "modulereader.h"
#include "modulefinder.h"
#include "modulefile.h"
#include "../object/type.h"
#include "../object/member.h"
#include "../object/field.h"
#include "../object/property.h"
#include "../object/method.h"
#include "../object/standardtypeinfo.h"
#include "../gc/gc.h"
#include "../util/stringbuffer.h"
#include "../debug/debugsymbols.h"
#include "../ee/thread.h"
#include "../ee/refsignature.h"
#include "../res/staticstrings.h"

namespace ovum
{

Module::Module(VM *vm, const PathName &fileName, ModuleParams &params) :
	// This initializer list is kind of silly
	name(params.name),
	version(params.version),
	fileName(fileName),
	staticState(nullptr),
	staticStateDeallocator(nullptr),
	// defs - initialized later
	functions(0),
	types(0),
	fields(0),
	methods(0),
	strings(0),
	members(params.globalMemberCount),
	// refs - initialized later
	moduleRefs(0),
	functionRefs(0),
	typeRefs(0),
	fieldRefs(0),
	methodRefs(0),
	nativeLib(),
	mainMethod(nullptr),
	debugData(nullptr),
	vm(vm),
	pool(vm->GetModulePool())
{ }

Module::~Module()
{
	// Note: Don't touch any of the string values.
	// They're managed by the GC, so we let her clean it up.

	if (staticStateDeallocator)
		staticStateDeallocator(staticState);
	FreeNativeLibrary();
}

Module *Module::FindModuleRef(String *name) const
{
	for (size_t i = 0; i < moduleRefs.GetLength(); i++)
		if (String_Equals(moduleRefs[i]->name, name))
			return moduleRefs[i];

	return nullptr;
}

bool Module::FindMember(String *name, bool includeInternal, GlobalMember &result) const
{
	GlobalMember member;
	if (!members.Get(name, member))
		return false;
	
	if (!includeInternal && member.IsInternal())
		return false;

	result = member;
	return true;
}

Type *Module::FindType(String *name, bool includeInternal) const
{
	GlobalMember member;
	if (!members.Get(name, member))
		return nullptr;

	if (!includeInternal && member.IsInternal())
		return nullptr;

	return member.GetType();
}

Method *Module::FindGlobalFunction(String *name, bool includeInternal) const
{
	GlobalMember member;
	if (!members.Get(name, member))
		return nullptr;

	if (!includeInternal && member.IsInternal())
		return nullptr;

	return member.GetFunction();
}

bool Module::FindConstant(String *name, bool includeInternal, Value &result) const
{
	GlobalMember member;
	if (!members.Get(name, member))
		return false;

	if (!includeInternal && member.IsInternal()
		||
		!member.IsConstant())
		return false;

	result = *member.GetConstant();
	return true;
}

#define TOKEN_INDEX(tok)  (((tok) & module_file::TOKEN_INDEX_MASK) - 1)

Module *Module::FindModuleRef(Token token) const
{
	namespace mf = module_file;
	OVUM_ASSERT((token & mf::TOKEN_KIND_MASK) == mf::TOKEN_MODULEREF);

	return moduleRefs[TOKEN_INDEX(token)];
}

Type *Module::FindType(Token token) const
{
	namespace mf = module_file;
	Token tokenKind = token & mf::TOKEN_KIND_MASK;
	OVUM_ASSERT(
		tokenKind == mf::TOKEN_TYPEDEF ||
		tokenKind == mf::TOKEN_TYPEREF
	);

	if (tokenKind == mf::TOKEN_TYPEDEF)
		return types[TOKEN_INDEX(token)].get();
	if (tokenKind == mf::TOKEN_TYPEREF)
		return typeRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

Method *Module::FindMethod(Token token) const
{
	namespace mf = module_file;
	Token tokenKind = token & mf::TOKEN_KIND_MASK;
	OVUM_ASSERT(
		tokenKind == mf::TOKEN_METHODDEF ||
		tokenKind == mf::TOKEN_METHODREF ||
		tokenKind == mf::TOKEN_FUNCTIONDEF ||
		tokenKind == mf::TOKEN_FUNCTIONREF
	);

	uint32_t idx = TOKEN_INDEX(token);

	if (tokenKind == mf::TOKEN_METHODDEF)
		return methods[idx].get();
	if (tokenKind == mf::TOKEN_METHODREF)
		return methodRefs[idx];
	if (tokenKind == mf::TOKEN_FUNCTIONDEF)
		return functions[idx].get();
	if (tokenKind == mf::TOKEN_FUNCTIONREF)
		return functionRefs[idx];

	return nullptr; // not found
}

Field *Module::FindField(Token token) const
{
	namespace mf = module_file;
	Token tokenKind = token & mf::TOKEN_KIND_MASK;
	OVUM_ASSERT(
		tokenKind == mf::TOKEN_FIELDDEF ||
		tokenKind == mf::TOKEN_FIELDREF
	);

	if (tokenKind == mf::TOKEN_FIELDDEF)
		return fields[TOKEN_INDEX(token)].get();
	if (tokenKind == mf::TOKEN_FIELDREF)
		return fieldRefs[TOKEN_INDEX(token)];

	return nullptr; // not found
}

String *Module::FindString(Token token) const
{
	namespace mf = module_file;
	OVUM_ASSERT((token & mf::TOKEN_KIND_MASK) == mf::TOKEN_STRING);

	if ((token & mf::TOKEN_KIND_MASK) == mf::TOKEN_STRING)
		return strings[TOKEN_INDEX(token)];

	return nullptr;
}

#undef TOKEN_INDEX

void *Module::FindNativeFunction(const char *name)
{
	if (os::LibraryHandleIsValid(&nativeLib))
		return FindNativeEntryPoint(name);
	return nullptr;
}

void Module::InitStaticState(void *state, StaticStateDeallocator deallocator)
{
	if (deallocator == nullptr)
		return;

	this->staticState = state;
	this->staticStateDeallocator = deallocator;
}

Module *Module::Open(
	VM *vm,
	const PathName &fileName,
	ModuleVersion *requiredVersion,
	PartiallyOpenedModulesList &partiallyOpenedModules
)
{
	ModulePool *pool = vm->GetModulePool();
	Module *outputModule = nullptr;

	try
	{
		ModuleReader reader(vm, partiallyOpenedModules);
		reader.Open(fileName);

		Box<Module> output = reader.ReadModule();

		debug::ModuleDebugData::TryLoad(fileName, output.get());

		outputModule = output.get();

		// ModulePool takes ownership of the module now
		pool->Add(std::move(output));
	}
	catch (ModuleIOException &ioError)
	{
		throw ModuleLoadException(fileName, ioError.what());
	}
	catch (std::bad_alloc&)
	{
		throw ModuleLoadException(fileName, "Out of memory");
	}

	return outputModule;
}

Module *Module::OpenByName(
	VM *vm,
	String *name,
	ModuleVersion *requiredVersion,
	PartiallyOpenedModulesList &partiallyOpenedModules
)
{
	Module *output;
	if (output = vm->GetModulePool()->Get(name, requiredVersion))
		return output;

	PathName moduleFileName(256);
	ModuleFinder finder(vm);

	bool found = finder.FindModulePath(name, requiredVersion, moduleFileName);

	if (!found)
	{
		moduleFileName.ReplaceWith(name);
		throw ModuleLoadException(moduleFileName, "Could not locate the module file.");
	}

	if (vm->verbose)
	{
		VM::Printf(L"Loading module '%ls' ", name);
		wprintf(L"from file '" OVUM_PATHNWF L"'\n", moduleFileName.GetDataPointer());
	}

	output = Open(
		vm,
		moduleFileName,
		requiredVersion,
		partiallyOpenedModules
	);

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
	os::LibraryStatus r = os::OpenLibrary(fileName.GetDataPointer(), &this->nativeLib);

	if (r != os::LIBRARY_OK)
		throw ModuleLoadException(path, "Could not load native library file.");
}

void *Module::FindNativeEntryPoint(const char *name)
{
	return os::FindLibraryFunction(&this->nativeLib, name);
}

void Module::FreeNativeLibrary()
{
	if (os::LibraryHandleIsValid(&nativeLib))
	{
		os::CloseLibrary(&nativeLib);
		nativeLib = os::LibraryHandle();
	}
}

void Module::TryRegisterStandardType(Type *type)
{
	VM *vm = this->vm;
	StandardTypeCollection *stdTypes = vm->GetStandardTypeCollection();

	StandardTypeInfo stdType;
	if (!stdTypes->Get(type->fullName, stdType))
		// This doesn't appear to be a standard type!
		return;

	if (vm->types.*(stdType.member) == nullptr)
	{
		vm->types.*(stdType.member) = type;

		if (stdType.extendedIniter != nullptr)
			stdType.extendedIniter(vm, this, type);
	}
}

} // namespace ovum

// Paper thin API wrapper functions, whoo!

OVUM_API ModuleHandle FindModule(ThreadHandle thread, String *name, ModuleVersion *version)
{
	ovum::ModulePool *pool = thread->GetVM()->GetModulePool();
	if (version == nullptr)
		return pool->Get(name);
	else
		return pool->Get(name, version);
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

OVUM_API void *Module_GetStaticState(ModuleHandle module)
{
	return module->GetStaticState();
}

OVUM_API void *Module_GetCurrentStaticState(ThreadHandle thread)
{
	const ovum::StackFrame *frame = thread->GetCurrentFrame();
	if (frame == nullptr || frame->method == nullptr)
		return nullptr;

	ovum::Module *module = frame->method->group->declModule;
	return module->GetStaticState();
}

OVUM_API void Module_InitStaticState(ModuleHandle module, void *state, StaticStateDeallocator deallocator)
{
	module->InitStaticState(state, deallocator);
}

OVUM_API bool Module_GetGlobalMember(ModuleHandle module, String *name, bool includeInternal, GlobalMember *result)
{
	ovum::GlobalMember member;
	if (module->FindMember(name, includeInternal, member))
	{
		member.ToPublicGlobalMember(result);
		return true;
	}
	return false;
}

OVUM_API size_t Module_GetGlobalMemberCount(ModuleHandle module)
{
	return module->GetMemberCount();
}

OVUM_API bool Module_GetGlobalMemberByIndex(ModuleHandle module, size_t index, GlobalMember *result)
{
	ovum::GlobalMember member;
	if (module->GetMemberByIndex(index, member))
	{
		member.ToPublicGlobalMember(result);
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

OVUM_API int Module_GetSearchDirectories(ThreadHandle thread, size_t resultSize, String **result, size_t *count)
{
	const size_t BUFFER_SIZE = 16;

	ovum::VM *vm = thread->GetVM();
	ovum::ModuleFinder finder(vm);

	// ModuleFinder returns directories as a PathName array, so we
	// need to fetch them into an intermediate container in order
	// to convert them to String*s.
	const ovum::PathName *paths[BUFFER_SIZE];
	size_t dirCount = finder.GetSearchDirectories(BUFFER_SIZE, paths);

	resultSize = min(resultSize, dirCount);
	for (size_t i = 0; i < resultSize; i++)
	{
		String *name = paths[i]->ToManagedString(thread);
		if (name == nullptr)
			return OVUM_ERROR_NO_MEMORY;
		*result++ = name;
	}

	*count = dirCount;
	RETURN_SUCCESS;
}
