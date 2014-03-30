#include "aves_enum.h"
#include "ov_stringbuffer.h"

AVES_API NATIVE_FUNCTION(aves_Enum_getHashCode)
{
	VM_PushInt(thread, THISV.integer);
}
AVES_API NATIVE_FUNCTION(aves_Enum_toString)
{
	// Try to find a field in the instance's type whose value
	// matches the instance's value, and return its name.

	TypeHandle thisType = THISV.type;

	TypeMemberIterator iter(thisType);
	while (iter.MoveNext())
	{
		FieldHandle field = Member_ToField(iter.Current());
		Value value;
		if (field && Field_GetStaticValue(field, &value) &&
			value.type == thisType && value.integer == THISV.integer)
		{
			VM_PushString(thread, Member_GetName((MemberHandle)field));
			return;
		}
	}

	// Nothing found, stringify the integer value instead
	VM_PushInt(thread, THISV.integer);
	VM_InvokeMember(thread, strings::toString, 0, nullptr);
}

AVES_API NATIVE_FUNCTION(aves_Enum_opEquals)
{
	if (args[0].type != args[1].type)
		VM_PushBool(thread, false);
	else
		VM_PushBool(thread, args[0].integer == args[1].integer);
}
AVES_API NATIVE_FUNCTION(aves_Enum_opCompare)
{
	if (args[0].type != args[1].type)
		VM_ThrowTypeError(thread);

	int64_t left = args[0].integer;
	int64_t right = args[0].integer;

	VM_PushInt(thread, left < right ? -1 :
		left > right ? 1 :
		0);
}
AVES_API NATIVE_FUNCTION(aves_Enum_opPlus)
{
	VM_PushInt(thread, THISV.integer);
}

AVES_API NATIVE_FUNCTION(aves_EnumSet_hasFlag)
{
	if (THISV.type != args[1].type)
		VM_ThrowTypeError(thread);

	VM_PushBool(thread, (THISV.integer & args[1].integer) == args[1].integer);
}
AVES_API NATIVE_FUNCTION(aves_EnumSet_toString)
{
	StringBuffer buf(thread, 256);

	int64_t remainingFlags = THISV.integer;
	bool foundField = false;

	TypeHandle thisType = THISV.type;

	TypeMemberIterator iter(thisType);
	while (iter.MoveNext())
	{
		FieldHandle field = Member_ToField(iter.Current());
		Value value;
		if (field && Field_GetStaticValue(field, &value) &&
			value.type == thisType)
		{
			// If the value matches THISV.integer entirely, we always
			// prefer the name of that field.
			if (THISV.integer == value.integer)
			{
				// Don't append the string to the buffer; we don't want
				// to allocate a whole new string for it upon returning.
				VM_PushString(thread, Member_GetName((MemberHandle)field));
				return; // Done!
			}
			// Let's see if the value matches any of the remaining flags,
			// and no other
			else if ((remainingFlags & value.integer) != 0 &&
				(~remainingFlags & value.integer) == 0)
			{
				// value.integer covers some or all of the remaining flags,
				// so let's append the name of that field!
				if (buf.GetLength() > 0)
					buf.Append(thread, 3, " | ");
				buf.Append(thread, Member_GetName((MemberHandle)field));
				remainingFlags &= ~(remainingFlags & value.integer);

				if (remainingFlags == 0)
					break; // Done!
			}
		}
	}

	if (remainingFlags != 0)
	{
		Value *remainingString = VM_Local(thread, 0);
		SetString(remainingString, integer::ToString(thread, remainingFlags, 10, 0, false));
		if (buf.GetLength() > 0)
		{
			buf.Append(thread, 3, " | ");
			buf.Append(thread, remainingString->common.string);
		}
		else
		{
			VM_Push(thread, *remainingString);
			return;
		}
	}

	VM_PushString(thread, buf.ToString(thread));
}

AVES_API NATIVE_FUNCTION(aves_EnumSet_opOr)
{
	if (args[0].type != args[1].type)
		VM_ThrowTypeError(thread);

	Value output;
	output.type = args[0].type;
	output.integer = args[0].integer | args[1].integer;
	VM_Push(thread, output);
}
AVES_API NATIVE_FUNCTION(aves_EnumSet_opXor)
{
	if (args[0].type != args[1].type)
		VM_ThrowTypeError(thread);

	Value output;
	output.type = args[0].type;
	output.integer = args[0].integer ^ args[1].integer;
	VM_Push(thread, output);
}
AVES_API NATIVE_FUNCTION(aves_EnumSet_opAnd)
{
	if (args[0].type != args[1].type)
		VM_ThrowTypeError(thread);

	Value output;
	output.type = args[0].type;
	output.integer = args[0].integer & args[1].integer;
	VM_Push(thread, output);
}
AVES_API NATIVE_FUNCTION(aves_EnumSet_opNot)
{
	Value output;
	output.type = args[0].type;
	output.integer = ~args[0].integer;
	VM_Push(thread, output);
}