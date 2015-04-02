#include "aves_boolean.h"

AVES_API NATIVE_FUNCTION(aves_Boolean_opEquals)
{
	// args[0] is guaranteed to be of type Boolean!
	VM_PushBool(thread,
		args[0].type == args[1].type &&
		!args[0].integer == !args[1].integer);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Boolean_opCompare)
{
	if (args[0].type != args[1].type)
		return VM_ThrowTypeError(thread, nullptr);

	int64_t left  = args[0].integer;
	int64_t right = args[1].integer;
	VM_PushInt(thread,
		left < right ? -1 :
		left > right ? 1 :
		0);
	RETURN_SUCCESS;
}