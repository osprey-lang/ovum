#include "aves_env.h"

FieldHandle EnvArgsField;

AVES_API void aves_Env_init(TypeHandle type)
{
	EnvArgsField = (FieldHandle)Type_GetMember(type, strings::_args);
}

AVES_API NATIVE_FUNCTION(aves_Env_snew)
{
	const int argCount = VM_GetArgCount();

	Value *argsList = VM_Local(thread, 0);
	VM_PushInt(thread, argCount); // list capacity
	GC_Construct(thread, GetType_List(), 1, argsList);

	VM_GetArgValues(argsList->common.list->values, argCount);

	VM_Push(thread, *argsList);
	VM_StoreStaticField(thread, EnvArgsField);
}

AVES_API NATIVE_FUNCTION(aves_Env_get_newline)
{
	VM_PushString(thread, strings::newline);
}

AVES_API NATIVE_FUNCTION(aves_Env_get_tickCount)
{
	VM_PushInt(thread, GetTickCount64());
}