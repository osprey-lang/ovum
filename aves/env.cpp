#include "aves_env.h"

Value *EnvArgsField;

AVES_API BEGIN_NATIVE_FUNCTION(aves_Env_get_args)
{
	if (EnvArgsField == nullptr)
	{
		const int argCount = VM_GetArgCount();

		EnvArgsField = GC_AddStaticReference(NULL_VALUE);
		VM_PushInt(thread, argCount); // list capacity
		CHECKED(GC_Construct(thread, GetType_List(), 1, EnvArgsField));

		VM_GetArgValues(argCount, EnvArgsField->common.list->values);
		EnvArgsField->common.list->length = argCount;
	}

	VM_Push(thread, *EnvArgsField);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Env_get_newline)
{
	VM_PushString(thread, strings::newline);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Env_get_tickCount)
{
	VM_PushInt(thread, GetTickCount64());
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Env_get_stackTrace)
{
	String *stackTrace;
	CHECKED_MEM(stackTrace = VM_GetStackTrace(thread));
	VM_PushString(thread, stackTrace);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Env_get_bigEndian)
{
	static const union
	{
		uint32_t u32;
		char bytes[4];
	} endianness = { 0xff000000 };

	VM_PushBool(thread, endianness.bytes[0] == 0xff);
	RETURN_SUCCESS;
}