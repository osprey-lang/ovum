#include "ov_stringbuffer.h"
#include "aves_stringbuffer.h"

#define _SB(v)	reinterpret_cast<StringBuffer*>(v.instance)

AVES_API void aves_StringBuffer_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(StringBuffer));
	Type_SetFinalizer(type, aves_StringBuffer_finalize);
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_new)
{
	StringBuffer *buf = _SB(THISV);

	if (!buf->Init())
		return VM_ThrowMemoryError(thread);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_newCap)
{
	StringBuffer *buf = _SB(THISV);

	CHECKED(IntFromValue(thread, args + 1));
	int64_t capacity = args[1].integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		CHECKED(GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr));
		return VM_Throw(thread);
	}

	if (!buf->Init((int32_t)capacity))
		return VM_ThrowMemoryError(thread);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_length)
{
	StringBuffer *buf = _SB(THISV);
	VM_PushInt(thread, buf->GetLength());
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_capacity)
{
	StringBuffer *buf = _SB(THISV);
	VM_PushInt(thread, buf->GetCapacity());
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendLine)
{
	StringBuffer *buf = _SB(THISV);
	if (!buf->Append(strings::newline))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISV);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_appendInternal)
{
	// appendInternal(value is String, times is Int)
	// (The public-facing methods ensure the types are correct)
	int64_t times = args[2].integer;

	if (times < 0 || times > INT32_MAX)
	{
		VM_PushString(thread, strings::times);
		CHECKED(GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr));
		return VM_Throw(thread);
	}

	StringBuffer *buf = _SB(THISV);
	String *str = args[1].common.string;

	for (int32_t i = 0; i < (int32_t)times; i++)
		if (!buf->Append(str))
			return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISV);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendCodepointInternal)
{
	// appendCodepointInternal(cp is Int)
	// The public-facing method makes sure the type is right,
	// and also range-checks the value

	StringBuffer *buf = _SB(THISV);
	wuchar codepoint = (wuchar)args[1].integer;

	if (codepoint > 0xFFFF)
	{
		// Surrogate pair, whoo!
		SurrogatePair pair = UC_ToSurrogatePair(codepoint);
		if (!buf->Append(2, (uchar*)&pair))
			return VM_ThrowMemoryError(thread);
	}
	else
		if (!buf->Append(1, (uchar)codepoint))
			return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISV);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_insertInternal)
{
	// insertInternal(index is Int, value is String)
	// (The public-facing methods ensure the types are correct)
	StringBuffer *buf = _SB(THISV);
	int64_t index = args[1].integer;

	if (index < 0 || index > buf->GetLength())
	{
		VM_PushString(thread, strings::index);
		CHECKED(GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr));
		return VM_Throw(thread);
	}

	if (!buf->Insert((int32_t)index, args[2].common.string))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISV);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_StringBuffer_clear)
{
	StringBuffer *buf = _SB(THISV);
	buf->Clear();
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_toString)
{
	String *result = _SB(THISV)->ToString(thread);
	if (!result)
		return VM_ThrowMemoryError(thread);
	VM_PushString(thread, result);
	RETURN_SUCCESS;
}

void aves_StringBuffer_finalize(void *basePtr)
{
	// NOTE: Do not delete the memory! Just call the destructor.
	// The GC allocated things, so we let it clean things up.
	((StringBuffer*)basePtr)->~StringBuffer();
}