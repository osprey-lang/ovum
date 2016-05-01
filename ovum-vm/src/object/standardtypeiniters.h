#pragma once

#include "../vm.h"

namespace ovum
{

class StandardTypeIniters
{
public:
	// Initialization for aves.Object.
	static int InitObjectType(VM *vm, Module *declModule, Type *type);

	// Initialization for aves.List.
	static int InitListType(VM *vm, Module *declModule, Type *type);

	// Initialization for aves.Hash.
	static int InitHashType(VM *vm, Module *declModule, Type *type);

	// Initialization for aves.reflection.Type.
	static int InitTypeType(VM *vm, Module *declModule, Type *type);

private:
	static const char *const ListIniterFunctionName;
	static const char *const HashIniterFunctionName;
	static const char *const TypeIniterFunctionName;
};

} // namespace ovum
