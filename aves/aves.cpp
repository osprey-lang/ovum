#include "aves.h"
#include "aves_state.h"
#include <ov_string.h>
#include <string>
#include <memory>

using namespace aves;

// Note: This is not declared in any header file. Only in this source file.
AVES_API void CDECL OvumModuleMain(ModuleHandle module)
{
	Aves::Init(module);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_print)
{
	if (IS_NULL(args[0]))
		SetString(thread, args, strings::Empty); // null prints like empty string
	else if (!IsString(thread, args))
		CHECKED(StringFromValue(thread, args));

	VM_PrintLn(args->v.string);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_exit)
{
	Aves *aves = Aves::Get(thread);

	int exitCode;
	if (args[0].type == aves->aves.Int)
		exitCode = (int)args[0].v.integer;
	else if (args[0].type == aves->aves.UInt)
		exitCode = (int)args[0].v.uinteger;
	else if (args[0].type == aves->aves.Real)
		exitCode = (int)args[0].v.real;
	else
		exitCode = 0;

	exit(exitCode);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_number_asInt)
{
	VM_PushInt(thread, THISV.v.integer);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_number_asUInt)
{
	VM_PushUInt(thread, THISV.v.uinteger);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_number_asReal)
{
	VM_PushReal(thread, THISV.v.real);
	RETURN_SUCCESS;
}