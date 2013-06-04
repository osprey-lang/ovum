#include "aves_object.h"

AVES_API NATIVE_FUNCTION(aves_Object_new)
{
	// Do nothing. The aves.Object constructor actually doesn't do anything.
	// But we still need to declare it.
}

AVES_API NATIVE_FUNCTION(aves_Object_getHashCode)
{
	// We shift down the address by 2 to counteract hash
	// collisions when there's alignment going on.
	// It's a cheap trick, but hey.

	if (sizeof(void*) == 8)
		VM_PushInt(thread, (int64_t)THISV.instance >> 2);
	else
		VM_PushInt(thread, (int32_t)THISV.instance >> 2);
}

AVES_API NATIVE_FUNCTION(aves_Object_toString)
{
	VM_PushString(thread, Type_GetFullName(THISV.type));
}

AVES_API NATIVE_FUNCTION(aves_Object_opEquals)
{
	Value a = args[0], b = args[1];

	if (a.type != b.type)
		VM_PushBool(thread, false);
	else if (Type_GetFlags(a.type) & TYPE_PRIMITIVE)
		VM_PushBool(thread, a.integer == b.integer);
	else
		VM_PushBool(thread, a.instance == b.instance);
}