#include "boolean.h"
#include "../aves_state.h"

AVES_API NATIVE_FUNCTION(aves_Boolean_opEquals)
{
	// args[0] is guaranteed to be of type Boolean!
	VM_PushBool(thread,
		args[0].type == args[1].type &&
		!args[0].v.integer == !args[1].v.integer);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Boolean_opCompare)
{
	using namespace aves;
	Aves *aves = Aves::Get(thread);

	if (args[0].type != args[1].type)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 0);

	int64_t left = args[0].v.integer;
	int64_t right = args[1].v.integer;
	VM_PushInt(thread,
		left < right ? -1 :
		left > right ? 1 :
		0);
	RETURN_SUCCESS;
}
