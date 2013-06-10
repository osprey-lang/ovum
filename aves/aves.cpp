#include <iostream>
#include "aves.h"

namespace
{
	LitString<18> _ArgumentError_Name  = { 18, 0, STR_STATIC,
		'a','v','e','s','.','A','r','g','u','m','e','n','t','E','r','r','o','r',0 };
	LitString<22> _ArgumentNullError_Name  = { 22, 0, STR_STATIC,
		'a','v','e','s','.','A','r','g','u','m','e','n','t','N','u','l','l','E','r','r','o','r',0 };
	LitString<23> _ArgumentRangeError_Name = { 23, 0, STR_STATIC,
		'a','v','e','s','.','A','r','g','u','m','e','n','t','R','a','n','g','e','E','r','r','o','r',0 };
	LitString<20> _UnicodeCategory_Name = { 20, 0, STR_STATIC,
		'a','v','e','s','.','U','n','i','c','o','d','e','C','a','t','e','g','o','r','y',0 };

	LitString<6> _format = { 6, 0, STR_STATIC, 'f','o','r','m','a','t' };
}

TypeHandle ArgumentError;
TypeHandle ArgumentNullError;
TypeHandle ArgumentRangeError;
TypeHandle UnicodeCategoryType;
String *ArgumentError_Name      = _S(_ArgumentError_Name);
String *ArgumentNullError_Name  = _S(_ArgumentNullError_Name);
String *ArgumentRangeError_Name = _S(_ArgumentRangeError_Name);
String *UnicodeCategory_Name    = _S(_UnicodeCategory_Name);
String *format = _S(_format);


AVES_API void aves_init(ModuleHandle module)
{
	ArgumentError = Module_FindType(module, ArgumentError_Name, true);
	ArgumentNullError  = Module_FindType(module, ArgumentNullError_Name,  true);
	ArgumentRangeError = Module_FindType(module, ArgumentRangeError_Name, true);
	UnicodeCategoryType = Module_FindType(module, UnicodeCategory_Name, true);
}


AVES_API NATIVE_FUNCTION(aves_print)
{
	Value value = args[0];
	if (IS_NULL(value))
	{
		std::cout << std::endl; // null prints like empty string
		return;
	}
	if (!IsString(value))
		value = StringFromValue(thread, value);

	VM_PrintLn(value.common.string);
}

AVES_API NATIVE_FUNCTION(aves_printf)
{
	if (IS_NULL(args[0]))
	{
		VM_PushString(thread, format);
		GC_Construct(thread, ArgumentNullError, 1, nullptr);
		VM_Throw(thread);
	}

	if (!IsString(args[0]))
		VM_ThrowTypeError(thread);

	Value *str = VM_Local(thread, 0);

	VM_Push(thread, args[0]); // String
	VM_Push(thread, args[1]); // List or Hash
	VM_InvokeMember(thread, format, 1, str);

	VM_PrintLn(str->common.string);
}

AVES_API NATIVE_FUNCTION(aves_exit)
{
	int exitCode;
	if (args[0].type == GetType_Int())
		exitCode = (int)args[0].integer;
	else if (args[0].type == GetType_UInt())
		exitCode = (int)args[0].uinteger;
	else if (args[0].type == GetType_Real())
		exitCode = (int)args[0].real;
	else
		exitCode = 0;

	exit(exitCode);
}