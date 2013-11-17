#include "aves_object.h"
#include "ov_stringbuffer.h"

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
	StringBuffer buf(thread, Type_GetFullName(THISV.type)->length + 16);

	buf.Append(thread, '<');
	buf.Append(thread, Type_GetFullName(THISV.type));
	buf.Append(thread, 4, " at ");

	Value *valueString = VM_Local(thread, 0);
	if ((Type_GetFlags(THISV.type) & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE)
		*valueString = integer::ToStringDecimal(thread, THISV.integer);
	else
	{
		buf.Append(thread, 2, "0x");
		*valueString = uinteger::ToStringHex(thread, (uint64_t)THISV.instance, false);

		// On 32-bit systems, make it 8 hex digits wide; on 64-bit, make it 16.
		// Make it so, zero padding!
		if (valueString->common.string->length < sizeof(void*) * 2)
			buf.Append(thread, sizeof(void*) * 2 - valueString->common.string->length, '0');
	}
	buf.Append(thread, valueString->common.string);

	buf.Append(thread, '>');

	VM_PushString(thread, buf.ToString(thread));
}
