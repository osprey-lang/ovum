#include "aves_env.h"

Value *EnvArgsField;

AVES_API NATIVE_FUNCTION(aves_Env_get_args)
{
	if (EnvArgsField == nullptr)
	{
		const int argCount = VM_GetArgCount();

		EnvArgsField = GC_AddStaticReference(NULL_VALUE);
		VM_PushInt(thread, argCount); // list capacity
		GC_Construct(thread, GetType_List(), 1, EnvArgsField);

		VM_GetArgValues(argCount, EnvArgsField->common.list->values);
		EnvArgsField->common.list->length = argCount;
	}

	VM_Push(thread, *EnvArgsField);
}

AVES_API NATIVE_FUNCTION(aves_Env_get_newline)
{
	VM_PushString(thread, strings::newline);
}

AVES_API NATIVE_FUNCTION(aves_Env_get_tickCount)
{
	VM_PushInt(thread, GetTickCount64());
}

AVES_API NATIVE_FUNCTION(aves_Env_get_stackTrace)
{
	VM_PushString(thread, VM_GetStackTrace(thread));
}