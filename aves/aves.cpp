#include <iostream>
#include "aves.h"

LitString<18> _ArgumentError_Name  = { 18, 0, StringFlags::STATIC,
	'a','v','e','s','.','A','r','g','u','m','e','n','t','E','r','r','o','r',0 };
LitString<22> _ArgumentNullError_Name  = { 22, 0, StringFlags::STATIC,
	'a','v','e','s','.','A','r','g','u','m','e','n','t','N','u','l','l','E','r','r','o','r',0 };
LitString<23> _ArgumentRangeError_Name = { 23, 0, StringFlags::STATIC,
	'a','v','e','s','.','A','r','g','u','m','e','n','t','R','a','n','g','e','E','r','r','o','r',0 };
LitString<22> _DuplicateKeyError_Name = { 22, 0, StringFlags::STATIC,
	'a','v','e','s','.','D','u','p','l','i','c','a','t','e','K','e','y','E','r','r','o','r',0 };
LitString<20> _UnicodeCategory_Name = { 20, 0, StringFlags::STATIC,
	'a','v','e','s','.','U','n','i','c','o','d','e','C','a','t','e','g','o','r','y',0 };
LitString<19> _BufferViewKind_Name = { 19, 0, StringFlags::STATIC,
	'a','v','e','s','.','B','u','f','f','e','r','V','i','e','w','K','i','n','d',0 };
LitString<14> _HashEntry_Name = { 14, 0, StringFlags::STATIC,
	'a','v','e','s','.','H','a','s','h','E','n','t','r','y',0 };

LitString<6> _format = { 6, 0, StringFlags::STATIC, 'f','o','r','m','a','t' };

TypeHandle ArgumentError;
TypeHandle ArgumentNullError;
TypeHandle ArgumentRangeError;
TypeHandle DuplicateKeyError;
TypeHandle UnicodeCategoryType;
TypeHandle BufferViewKindType;
TypeHandle HashEntryType;
String *format = _S(_format);


AVES_API void aves_init(ModuleHandle module)
{
	ArgumentError       = Module_FindType(module, _S(_ArgumentError_Name),      true);
	ArgumentNullError   = Module_FindType(module, _S(_ArgumentNullError_Name),  true);
	ArgumentRangeError  = Module_FindType(module, _S(_ArgumentRangeError_Name), true);
	DuplicateKeyError   = Module_FindType(module, _S(_DuplicateKeyError_Name),  true);
	UnicodeCategoryType = Module_FindType(module, _S(_UnicodeCategory_Name),    true);
	BufferViewKindType  = Module_FindType(module, _S(_BufferViewKind_Name),     true);
	HashEntryType       = Module_FindType(module, _S(_HashEntry_Name),          true);
}


AVES_API NATIVE_FUNCTION(aves_print)
{
	Value value = args[0];
	if (IS_NULL(value))
	{
		std::wcout << std::endl; // null prints like empty string
		return;
	}
	if (!IsString(value))
		value = StringFromValue(thread, value);

	VM_PrintLn(value.common.string);
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

AVES_API NATIVE_FUNCTION(aves_number_asInt)
{
	VM_PushInt(thread, THISV.integer);
}
AVES_API NATIVE_FUNCTION(aves_number_asUInt)
{
	VM_PushUInt(thread, THISV.uinteger);
}
AVES_API NATIVE_FUNCTION(aves_number_asReal)
{
	VM_PushReal(thread, THISV.real);
}