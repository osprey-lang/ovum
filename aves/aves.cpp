#include <string>
#include <memory>
#include "aves.h"
#include "ov_string.h"

LitString<8> _Int_Name =  { 8, 0, StringFlags::STATIC, 'a','v','e','s','.','I','n','t',0 };
LitString<9> _UInt_Name = { 9, 0, StringFlags::STATIC, 'a','v','e','s','.','U','I','n','t',0 };
LitString<9> _Real_Name = { 9, 0, StringFlags::STATIC, 'a','v','e','s','.','R','e','a','l',0 };

LitString<18> _ArgumentError_Name = { 18, 0, StringFlags::STATIC,
	'a','v','e','s','.','A','r','g','u','m','e','n','t','E','r','r','o','r',0 };
LitString<22> _ArgumentNullError_Name = { 22, 0, StringFlags::STATIC,
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
LitString<17> _ConsoleColor_Name  ={ 17, 0, StringFlags::STATIC,
	'a','v','e','s','.','C','o','n','s','o','l','e','C','o','l','o','r',0 };
LitString<15> _ConsoleKey_Name = { 15, 0, StringFlags::STATIC,
	'a','v','e','s','.','C','o','n','s','o','l','e','K','e','y',0 };
LitString<19> _ConsoleKeyCode_Name = { 19, 0, StringFlags::STATIC,
	'a','v','e','s','.','C','o','n','s','o','l','e','K','e','y','C','o','d','e',0 };

LitString<6> _format = { 6, 0, StringFlags::STATIC, 'f','o','r','m','a','t' };

TypeHandle Types::Int;
TypeHandle Types::UInt;
TypeHandle Types::Real;
TypeHandle Types::ArgumentError;
TypeHandle Types::ArgumentNullError;
TypeHandle Types::ArgumentRangeError;
TypeHandle Types::DuplicateKeyError;
TypeHandle Types::UnicodeCategory;
TypeHandle Types::BufferViewKind;
TypeHandle Types::HashEntry;
TypeHandle Types::ConsoleColor;
TypeHandle Types::ConsoleKey;
TypeHandle Types::ConsoleKeyCode;
String *format = _S(_format);


// Note: This is not declared in any header file. Only in this source file.
AVES_API void OvumModuleMain(ModuleHandle module)
{
	Types::Int                = Module_FindType(module, _S(_Int_Name),                true);
	Types::UInt               = Module_FindType(module, _S(_UInt_Name),               true);
	Types::Real               = Module_FindType(module, _S(_Real_Name),               true);
	Types::ArgumentError      = Module_FindType(module, _S(_ArgumentError_Name),      true);
	Types::ArgumentNullError  = Module_FindType(module, _S(_ArgumentNullError_Name),  true);
	Types::ArgumentRangeError = Module_FindType(module, _S(_ArgumentRangeError_Name), true);
	Types::DuplicateKeyError  = Module_FindType(module, _S(_DuplicateKeyError_Name),  true);
	Types::UnicodeCategory    = Module_FindType(module, _S(_UnicodeCategory_Name),    true);
	Types::BufferViewKind     = Module_FindType(module, _S(_BufferViewKind_Name),     true);
	Types::HashEntry          = Module_FindType(module, _S(_HashEntry_Name),          true);
	Types::ConsoleColor       = Module_FindType(module, _S(_ConsoleColor_Name),       true);
	Types::ConsoleKey         = Module_FindType(module, _S(_ConsoleKey_Name),         true);
	Types::ConsoleKeyCode     = Module_FindType(module, _S(_ConsoleKeyCode_Name),     true);
}


AVES_API NATIVE_FUNCTION(aves_print)
{
	if (IS_NULL(*args))
		SetString(args, strings::Empty); // null prints like empty string
	else if (!IsString(*args))
		*args = StringFromValue(thread, *args);

	VM_PrintLn(args->common.string);
}

AVES_API NATIVE_FUNCTION(aves_exit)
{
	int exitCode;
	if (args[0].type == Types::Int)
		exitCode = (int)args[0].integer;
	else if (args[0].type == Types::UInt)
		exitCode = (int)args[0].uinteger;
	else if (args[0].type == Types::Real)
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