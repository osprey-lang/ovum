#include "standardtypeiniters.h"
#include "type.h"
#include "../module/module.h"

namespace ovum
{

const char *const StandardTypeIniters::ListIniterFunctionName = "InitListInstance";
const char *const StandardTypeIniters::ListConcatenatorFunctionName = "ConcatenateLists";
const char *const StandardTypeIniters::HashIniterFunctionName = "InitHashInstance";
const char *const StandardTypeIniters::HashConcatenatorFunctionName = "ConcatenateHashes";
const char *const StandardTypeIniters::TypeIniterFunctionName = "InitTypeToken";

int StandardTypeIniters::InitObjectType(VM *vm, Module *declModule, Type *type)
{
	if (type->GetTotalSize() > 0)
		throw ModuleLoadException(declModule->GetFileName(), "The type aves.Object must have a size of 0.");

	RETURN_SUCCESS;
}

int StandardTypeIniters::InitListType(VM *vm, Module *declModule, Type *type)
{
	void *initListInstance = declModule->FindNativeFunction(ListIniterFunctionName);
	void *concatLists = declModule->FindNativeFunction(ListConcatenatorFunctionName);

	if (initListInstance == nullptr)
		throw ModuleLoadException(declModule->GetFileName(), "Missing native initializer function for aves.List.");
	if (concatLists == nullptr)
		throw ModuleLoadException(declModule->GetFileName(), "Missing native function to concatenate lists.");

	vm->functions.initListInstance = (ListInitializer)initListInstance;
	vm->functions.concatLists = (ValueConcatenator)concatLists;

	RETURN_SUCCESS;
}

int StandardTypeIniters::InitHashType(VM *vm, Module *declModule, Type *type)
{
	void *initHashInstance = declModule->FindNativeFunction(HashIniterFunctionName);
	void *concatHashes = declModule->FindNativeFunction(HashConcatenatorFunctionName);

	if (initHashInstance == nullptr)
		throw ModuleLoadException(declModule->GetFileName(), "Missing native initializer function for aves.Hash.");

	if (concatHashes == nullptr)
		throw ModuleLoadException(declModule->GetFileName(), "Missing native function to concatenate hash tables.");

	vm->functions.initHashInstance = (HashInitializer)initHashInstance;
	vm->functions.concatHashes = (ValueConcatenator)concatHashes;

	RETURN_SUCCESS;
}

int StandardTypeIniters::InitTypeType(VM *vm, Module *declModule, Type *type)
{
	void *initTypeToken = declModule->FindNativeFunction(TypeIniterFunctionName);

	if (initTypeToken == nullptr)
		throw ModuleLoadException(declModule->GetFileName(), "Missing native initializer function for aves.reflection.Type.");

	vm->functions.initTypeToken = (TypeTokenInitializer)initTypeToken;

	RETURN_SUCCESS;
}

} // namespace ovum
