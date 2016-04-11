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

	if (initListInstance == nullptr)
		throw ModuleLoadException(declModule->GetFileName(), "Missing native initializer function for aves.List.");

	vm->functions.initListInstance = (ListInitializer)initListInstance;

	RETURN_SUCCESS;
}

int StandardTypeIniters::InitHashType(VM *vm, Module *declModule, Type *type)
{
	void *initHashInstance = declModule->FindNativeFunction(HashIniterFunctionName);

	if (initHashInstance == nullptr)
		throw ModuleLoadException(declModule->GetFileName(), "Missing native initializer function for aves.Hash.");

	vm->functions.initHashInstance = (HashInitializer)initHashInstance;

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
