#include "aves_env.h"
#include <memory>
#if OVUM_UNIX
#include <time.h>
#endif

Value *EnvArgsField;

AVES_API BEGIN_NATIVE_FUNCTION(aves_Env_get_args)
{
	if (EnvArgsField == nullptr)
	{
		const int argCount = VM_GetArgCount(thread);

		CHECKED_MEM(EnvArgsField = GC_AddStaticReference(thread, NULL_VALUE));
		VM_PushInt(thread, argCount); // list capacity
		CHECKED(GC_Construct(thread, GetType_List(thread), 1, EnvArgsField));

		VM_GetArgValues(thread, argCount, EnvArgsField->common.list->values);
		EnvArgsField->common.list->length = argCount;
	}

	VM_Push(thread, EnvArgsField);
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

AVES_API BEGIN_NATIVE_FUNCTION(aves_Env_get_currentDirectory)
{
#if OVUM_WINDOWS
	String *result = nullptr;
	do
	{
		DWORD pathLength = GetCurrentDirectoryW(0, nullptr);
		if (pathLength <= MAX_PATH)
		{
			WCHAR buf[MAX_PATH];
			pathLength = GetCurrentDirectoryW(MAX_PATH, buf);
			if (pathLength < MAX_PATH)
			{
				result = GC_ConstructString(thread, (int32_t)pathLength, (const uchar*)buf);
				break;
			}
		}
		else
		{
			std::unique_ptr<WCHAR[]> buf(new(std::nothrow) WCHAR[pathLength]);
			if (buf.get() == nullptr)
				return VM_ThrowMemoryError(thread);

			DWORD charsWritten = GetCurrentDirectoryW(pathLength, buf.get());
			if (pathLength < charsWritten)
			{
				result = GC_ConstructString(thread, (int32_t)charsWritten, (const uchar*)buf.get());
				break;
			}
		}
	} while (true);

	// If we reach this point and result is null, it can only mean that
	// GC_ConstructString failed, so let's throw a party or something!
	CHECKED_MEM(result);

	VM_PushString(thread, result);
#else
#error Not supported
#endif
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Env_get_newline)
{
	VM_PushString(thread, strings::newline);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Env_get_stackTrace)
{
	String *stackTrace;
	CHECKED_MEM(stackTrace = VM_GetStackTrace(thread));
	VM_PushString(thread, stackTrace);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Env_get_tickCount)
{
#if OVUM_WINDOWS
	VM_PushInt(thread, GetTickCount64());
#else
	timespec t;
	int r = clock_gettime(CLOCK_MONOTONIC, &t);
	if (r == -1)
		// TODO: POSIX error codes
		VM_ThrowError(thread);
	int64_t milliseconds = (int64_t)t.tv_sec * 1000;
	milliseconds += t.tv_nsec / 1000000;
	VM_PushInt(thread, milliseconds);
#endif
	RETURN_SUCCESS;
}