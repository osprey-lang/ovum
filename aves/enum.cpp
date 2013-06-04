#include "aves_enum.h"

AVES_API NATIVE_FUNCTION(aves_Enum_getHashCode)
{
	VM_PushInt(thread, THISV.integer);
}
AVES_API NATIVE_FUNCTION(aves_Enum_toString)
{
	VM_PushInt(thread, THISV.integer);
	VM_InvokeMember(thread, strings::toString, 0, NULL);
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