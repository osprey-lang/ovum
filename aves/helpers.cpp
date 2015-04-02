#include "aves_helpers.h"

AVES_API BEGIN_NATIVE_FUNCTION(aves_helpers_loadMember)
{
	VM_Push(thread, args + 0);
	CHECKED(VM_LoadMember(thread, args[1].common.string, nullptr));
}
END_NATIVE_FUNCTION