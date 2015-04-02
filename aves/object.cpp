#include "aves_object.h"
#include "ov_stringbuffer.h"

AVES_API NATIVE_FUNCTION(aves_Object_new)
{
	// Do nothing. The aves.Object constructor actually doesn't do anything.
	// But we still need to declare it.
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Object_getHashCode)
{
	VM_PushInt(thread, GC_GetObjectHashCode(THISP));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Object_toString)
{
	StringBuffer buf;
	CHECKED_MEM(buf.Init(Type_GetFullName(THISV.type)->length + 16));

	CHECKED_MEM(buf.Append('<'));
	CHECKED_MEM(buf.Append(Type_GetFullName(THISV.type)));
	CHECKED_MEM(buf.Append(' '));

	String *valueString;
	if ((Type_GetFlags(THISV.type) & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE)
		valueString = integer::ToString(thread, THISV.integer, 10, 0, false);
	else
		valueString = uinteger::ToString(thread, GC_GetObjectHashCode(THISP), 16, 8, false);
	CHECKED_MEM(valueString);
	CHECKED_MEM(buf.Append(valueString));

	CHECKED_MEM(buf.Append('>'));

	// Reuse valueString for the return value
	CHECKED_MEM(valueString = buf.ToString(thread));
	VM_PushString(thread, valueString);
}
END_NATIVE_FUNCTION