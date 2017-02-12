#include "env.h"
#include "../tempbuffer.h"
#include <memory>
#if OVUM_UNIX
#include <time.h>
#endif

Value *EnvArgsField;

AVES_API BEGIN_NATIVE_FUNCTION(aves_Env_get_args)
{
	if (EnvArgsField == nullptr)
	{
		size_t argCount = VM_GetArgCount(thread);

		Value nullValue = NULL_VALUE;
		CHECKED_MEM(EnvArgsField = GC_AddStaticReference(thread, &nullValue));

		VM_PushInt(thread, (int64_t)argCount); // list capacity
		CHECKED(GC_Construct(thread, GetType_List(thread), 1, EnvArgsField));

		VM_GetArgValues(thread, argCount, EnvArgsField->v.list->values);
		EnvArgsField->v.list->length = argCount;
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

	aves::TempBuffer<WCHAR, MAX_PATH> buf;
	// We need to keep trying to read the current directory until we
	// succeed, because the current directory may change between calls
	// to GetCurrentDirectory.
	// Under the overwhelming majority of conceivable circumstances,
	// a single iteration should be enough. But in particularly aberrant
	// cases, we may need to run this two or three times...
	while (true)
	{
		// MSDN is silent on whether the first argument to GetCurrentDirectory
		// (the buffer length) includes space for the terminating \0 or not.
		// Some quick testing and googling seems to indicate that it does NOT
		// include space for \0, meaning if you pass 100 and the buffer is 100
		// characters in size (excluding \0), Windows will attempt to write to
		// offset 100... and (probably) break something.
		// For that reason, we have to subtract 1 from the buffer capacity.
		size_t pathLength = (size_t)GetCurrentDirectoryW(
			(DWORD)buf.GetCapacity() - 1,
			buf.GetPointer()
		);
		// If the specified buffer capacity was sufficient, pathLength will now
		// contain the length of the path EXCLUDING \0. If the buffer was too
		// small, it'll instead be the required buffer size INCLUDING \0.
		//
		// Why do you do this, Microsoft. Ugh.

		if (pathLength < buf.GetCapacity())
		{
			// The buffer was big enough! Turn it into a String* so we can
			// escape from this madness.
			CHECKED_MEM(result = GC_ConstructString(
				thread,
				pathLength,
				reinterpret_cast<ovchar_t*>(buf.GetPointer())
			));
			break;
		}
		// Insufficient buffer, try to grow it.
		// Remember: pathLength INCLUDES the \0, so no need to +1.
		CHECKED_MEM(buf.EnsureCapacity((size_t)pathLength, false));
	}

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
