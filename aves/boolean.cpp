#include "aves_boolean.h"

LitString<5> _falseString = { 5, 0, STR_STATIC, 'f','a','l','s','e',0 };
LitString<4> _trueString  = { 4, 0, STR_STATIC, 't','r','u','e',0 };

String *falseString = _S(falseString);
String *trueString  = _S(trueString);

AVES_API NATIVE_FUNCTION(aves_bool)
{
	bool truthy = IsTrue(args[0]);
	VM_PushBool(thread, truthy);
}

AVES_API NATIVE_FUNCTION(aves_Boolean_getHashCode)
{
	VM_PushInt(thread, args[0].integer ? 1 : 0);
}

AVES_API NATIVE_FUNCTION(aves_Boolean_toString)
{
	VM_PushString(thread, args[0].integer ? trueString : falseString);
}

AVES_API NATIVE_FUNCTION(aves_Boolean_opEquals)
{
	// args[0] is guaranteed to be of type Boolean!
	VM_PushBool(thread,
		args[0].type == args[1].type &&
		!args[0].integer == !args[1].integer);
}

AVES_API NATIVE_FUNCTION(aves_Boolean_opCompare)
{
	if (args[0].type != args[1].type)
		VM_ThrowTypeError(thread, NULL);

	int64_t left  = args[0].integer;
	int64_t right = args[1].integer;
	VM_PushInt(thread,
		left < right ? -1 :
		left > right ? 1 :
		0);
}

AVES_API NATIVE_FUNCTION(aves_Boolean_opPlus)
{
	VM_PushInt(thread, args[0].integer ? 1 : 0);
}