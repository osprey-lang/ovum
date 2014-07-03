#include <string>
#include <memory>
#include "aves.h"
#include "ov_string.h"

#define SFS  ::StringFlags::STATIC
#define AVES 'a','v','e','s','.'

namespace type_names
{
	LitString<8> Int     = { 8,  0, SFS, AVES,'I','n','t',0 };
	LitString<9> UInt    = { 9,  0, SFS, AVES,'U','I','n','t',0 };
	LitString<9> Real    = { 9,  0, SFS, AVES,'R','e','a','l',0 };
	LitString<9> Char    = { 9,  0, SFS, AVES,'C','h','a','r',0 };
	LitString<11> String = { 11, 0, SFS, AVES,'S','t','r','i','n','g',0 };

	LitString<18> ArgumentError      = { 18, 0, SFS, AVES,'A','r','g','u','m','e','n','t','E','r','r','o','r',0 };
	LitString<22> ArgumentNullError  = { 22, 0, SFS, AVES,'A','r','g','u','m','e','n','t','N','u','l','l','E','r','r','o','r',0 };
	LitString<23> ArgumentRangeError = { 23, 0, SFS, AVES,'A','r','g','u','m','e','n','t','R','a','n','g','e','E','r','r','o','r',0 };
	LitString<22> DuplicateKeyError  = { 22, 0, SFS, AVES,'D','u','p','l','i','c','a','t','e','K','e','y','E','r','r','o','r',0 };
	LitString<22> InvalidStateError  = { 22, 0, SFS, AVES,'I','n','v','a','l','i','d','S','t','a','t','e','E','r','r','o','r',0 };
	LitString<22> NotSupportedError  = { 22, 0, SFS, AVES,'N','o','t','S','u','p','p','o','r','t','e','d','E','r','r','o','r',0 };
	LitString<20> UnicodeCategory    = { 20, 0, SFS, AVES,'U','n','i','c','o','d','e','C','a','t','e','g','o','r','y',0 };
	LitString<19> BufferViewKind     = { 19, 0, SFS, AVES,'B','u','f','f','e','r','V','i','e','w','K','i','n','d',0 };
	LitString<14> HashEntry          = { 14, 0, SFS, AVES,'H','a','s','h','E','n','t','r','y',0 };
	LitString<17> ConsoleColor       = { 17, 0, SFS, AVES,'C','o','n','s','o','l','e','C','o','l','o','r',0 };
	LitString<15> ConsoleKey         = { 15, 0, SFS, AVES,'C','o','n','s','o','l','e','K','e','y',0 };
	LitString<19> ConsoleKeyCode     = { 19, 0, SFS, AVES,'C','o','n','s','o','l','e','K','e','y','C','o','d','e',0 };

	LitString<10> IOError           = { 10, 0, SFS, 'i','o','.','I','O','E','r','r','o','r',0 };
	LitString<20> FileNotFoundError = { 20, 0, SFS, 'i','o','.','F','i','l','e','N','o','t','F','o','u','n','d','E','r','r','o','r',0 };
}

TypeHandle Types::Int;
TypeHandle Types::UInt;
TypeHandle Types::Real;
TypeHandle Types::Char;
TypeHandle Types::String;
TypeHandle Types::ArgumentError;
TypeHandle Types::ArgumentNullError;
TypeHandle Types::ArgumentRangeError;
TypeHandle Types::DuplicateKeyError;
TypeHandle Types::InvalidStateError;
TypeHandle Types::NotSupportedError;
TypeHandle Types::UnicodeCategory;
TypeHandle Types::BufferViewKind;
TypeHandle Types::HashEntry;
TypeHandle Types::ConsoleColor;
TypeHandle Types::ConsoleKey;
TypeHandle Types::ConsoleKeyCode;

TypeHandle Types::IOError;
TypeHandle Types::FileNotFoundError;


// Note: This is not declared in any header file. Only in this source file.
AVES_API void CDECL OvumModuleMain(ModuleHandle module)
{
	Types::Int                = Module_FindType(module, _S(type_names::Int),                true);
	Types::UInt               = Module_FindType(module, _S(type_names::UInt),               true);
	Types::Real               = Module_FindType(module, _S(type_names::Real),               true);
	Types::Char               = Module_FindType(module, _S(type_names::Char),               true);
	Types::String             = Module_FindType(module, _S(type_names::String),             true);
	Types::ArgumentError      = Module_FindType(module, _S(type_names::ArgumentError),      true);
	Types::ArgumentNullError  = Module_FindType(module, _S(type_names::ArgumentNullError),  true);
	Types::ArgumentRangeError = Module_FindType(module, _S(type_names::ArgumentRangeError), true);
	Types::DuplicateKeyError  = Module_FindType(module, _S(type_names::DuplicateKeyError),  true);
	Types::InvalidStateError  = Module_FindType(module, _S(type_names::InvalidStateError),  true);
	Types::NotSupportedError  = Module_FindType(module, _S(type_names::NotSupportedError),  true);
	Types::UnicodeCategory    = Module_FindType(module, _S(type_names::UnicodeCategory),    true);
	Types::BufferViewKind     = Module_FindType(module, _S(type_names::BufferViewKind),     true);
	Types::HashEntry          = Module_FindType(module, _S(type_names::HashEntry),          true);
	Types::ConsoleColor       = Module_FindType(module, _S(type_names::ConsoleColor),       true);
	Types::ConsoleKey         = Module_FindType(module, _S(type_names::ConsoleKey),         true);
	Types::ConsoleKeyCode     = Module_FindType(module, _S(type_names::ConsoleKeyCode),     true);
	Types::IOError            = Module_FindType(module, _S(type_names::IOError),            true);
	Types::FileNotFoundError  = Module_FindType(module, _S(type_names::FileNotFoundError),  true);
}


AVES_API BEGIN_NATIVE_FUNCTION(aves_print)
{
	if (IS_NULL(*args))
		SetString(args, strings::Empty); // null prints like empty string
	else if (!IsString(*args))
		CHECKED(StringFromValue(thread, args));

	VM_PrintLn(args->common.string);
}
END_NATIVE_FUNCTION

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
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_number_asInt)
{
	VM_PushInt(thread, THISV.integer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_number_asUInt)
{
	VM_PushUInt(thread, THISV.uinteger);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_number_asReal)
{
	VM_PushReal(thread, THISV.real);
	RETURN_SUCCESS;
}