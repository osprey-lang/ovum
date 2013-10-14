#include "aves_helpers.h"

AVES_API NATIVE_FUNCTION(aves_helpers_loadMember)
{
	VM_Push(thread, args[0]);
	VM_LoadMember(thread, args[1].common.string, nullptr);
}