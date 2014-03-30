#include "aves_object.h"
#include "ov_stringbuffer.h"

AVES_API NATIVE_FUNCTION(aves_Object_new)
{
	// Do nothing. The aves.Object constructor actually doesn't do anything.
	// But we still need to declare it.
}

AVES_API NATIVE_FUNCTION(aves_Object_getHashCode)
{
	VM_PushInt(thread, GC_GetObjectHashCode(THISP));
}

AVES_API NATIVE_FUNCTION(aves_Object_toString)
{
	StringBuffer buf(thread, Type_GetFullName(THISV.type)->length + 16);

	buf.Append(thread, '<');
	buf.Append(thread, Type_GetFullName(THISV.type));
	buf.Append(thread, ' ');

	String *valueString;
	if ((Type_GetFlags(THISV.type) & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE)
		valueString = integer::ToString(thread, THISV.integer, 10, 0, false);
	else
		valueString = uinteger::ToString(thread, GC_GetObjectHashCode(THISP), 16, 8, false);
	buf.Append(thread, valueString);

	buf.Append(thread, '>');

	VM_PushString(thread, buf.ToString(thread));
}