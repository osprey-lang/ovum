#include "aves_enum.h"
#include "ov_stringbuffer.h"

AVES_API NATIVE_FUNCTION(aves_Enum_getHashCode)
{
	VM_PushInt(thread, THISV.v.integer);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_Enum_toString)
{
	// Try to find a field in the instance's type whose value
	// matches the instance's value, and return its name.

	TypeHandle thisType = THISV.type;

	TypeMemberIterator iter(thisType);
	while (iter.MoveNext())
	{
		FieldHandle field = Member_ToField(iter.Current());
		if (field)
		{
			Value value;
			CHECKED(VM_LoadStaticField(thread, field, &value));
			if (value.type == thisType && value.v.integer == THISV.v.integer)
			{
				VM_PushString(thread, Member_GetName((MemberHandle)field));
				RETURN_SUCCESS;
			}
		}
	}

	// Nothing found, stringify the integer value instead
	VM_PushInt(thread, THISV.v.integer);
	CHECKED(VM_InvokeMember(thread, strings::toString, 0, nullptr));
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Enum_opEquals)
{
	if (args[0].type != args[1].type)
		VM_PushBool(thread, false);
	else
		VM_PushBool(thread, args[0].v.integer == args[1].v.integer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Enum_opCompare)
{
	if (args[0].type != args[1].type)
		return VM_ThrowTypeError(thread);

	int64_t left = args[0].v.integer;
	int64_t right = args[0].v.integer;

	VM_PushInt(thread, left < right ? -1 :
		left > right ? 1 :
		0);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Enum_opPlus)
{
	VM_PushInt(thread, THISV.v.integer);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_EnumSet_hasFlag)
{
	if (THISV.type != args[1].type)
		return VM_ThrowTypeError(thread);

	VM_PushBool(thread, (THISV.v.integer & args[1].v.integer) == args[1].v.integer);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_EnumSet_toString)
{
	StringBuffer buf;
	CHECKED_MEM(buf.Init(256));

	int64_t remainingFlags = THISV.v.integer;
	bool foundField = false;

	TypeHandle thisType = THISV.type;

	TypeMemberIterator iter(thisType);
	while (iter.MoveNext())
	{
		FieldHandle field = Member_ToField(iter.Current());
		if (field)
		{
			Value value;
			CHECKED(VM_LoadStaticField(thread, field, &value));
			if (value.type == thisType)
			{
				// If the value matches THISV.integer entirely, we always
				// prefer the name of that field.
				if (THISV.v.integer == value.v.integer)
				{
					// Don't append the string to the buffer; we don't want
					// to allocate a whole new string for it upon returning.
					VM_PushString(thread, Member_GetName((MemberHandle)field));
					RETURN_SUCCESS; // Done!
				}
				// Let's see if the value matches any of the remaining flags,
				// and no other
				else if ((remainingFlags & value.v.integer) != 0 &&
					(~remainingFlags & value.v.integer) == 0)
				{
					// value.integer covers some or all of the remaining flags,
					// so let's append the name of that field!
					if (buf.GetLength() > 0)
						CHECKED_MEM(buf.Append(3, " | "));
					CHECKED_MEM(buf.Append(Member_GetName((MemberHandle)field)));
					remainingFlags &= ~(remainingFlags & value.v.integer);

					if (remainingFlags == 0)
						break; // Done!
				}
			}
		}
	}

	if (remainingFlags != 0)
	{
		String *remainingString;
		CHECKED_MEM(remainingString = integer::ToString(thread, remainingFlags, 10, 0, false));
		SetString(thread, VM_Local(thread, 0), remainingString);
		if (buf.GetLength() > 0)
		{
			CHECKED_MEM(buf.Append(3, " | "));
			CHECKED_MEM(buf.Append(remainingString));
		}
		else
		{
			VM_PushString(thread, remainingString);
			RETURN_SUCCESS;
		}
	}

	String *result;
	CHECKED_MEM(result = buf.ToString(thread));
	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_EnumSet_opOr)
{
	if (args[0].type != args[1].type)
		return VM_ThrowTypeError(thread);

	Value output;
	output.type = args[0].type;
	output.v.integer = args[0].v.integer | args[1].v.integer;
	VM_Push(thread, &output);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_EnumSet_opXor)
{
	if (args[0].type != args[1].type)
		return VM_ThrowTypeError(thread);

	Value output;
	output.type = args[0].type;
	output.v.integer = args[0].v.integer ^ args[1].v.integer;
	VM_Push(thread, &output);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_EnumSet_opAnd)
{
	if (args[0].type != args[1].type)
		return VM_ThrowTypeError(thread);

	Value output;
	output.type = args[0].type;
	output.v.integer = args[0].v.integer & args[1].v.integer;
	VM_Push(thread, &output);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_EnumSet_opNot)
{
	Value output;
	output.type = args[0].type;
	output.v.integer = ~args[0].v.integer;
	VM_Push(thread, &output);
	RETURN_SUCCESS;
}