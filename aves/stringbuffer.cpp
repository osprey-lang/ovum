#include "ov_stringbuffer.h"
#include "aves_stringbuffer.h"
#include <new> // For placement new

#define _SB(v)	reinterpret_cast<StringBuffer*>(v.instance)

AVES_API void aves_StringBuffer_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(StringBuffer));
	Type_SetFinalizer(type, aves_StringBuffer_finalize);
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_new)
{
	StringBuffer *buf = _SB(THISV);

	new(buf) StringBuffer(thread);
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_newCap)
{
	StringBuffer *buf = _SB(THISV);

	IntFromValue(thread, args + 1);
	int64_t capacity = args[1].integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	new(buf) StringBuffer(thread, (int32_t)capacity);
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_length)
{
	StringBuffer *buf = _SB(THISV);
	VM_PushInt(thread, buf->GetLength());
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_capacity)
{
	StringBuffer *buf = _SB(THISV);
	VM_PushInt(thread, buf->GetCapacity());
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendLine)
{
	StringBuffer *buf = _SB(THISV);
	buf->Append(thread, strings::newline);

	VM_Push(thread, THISV);
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendInternal)
{
	// appendInternal(value is String, times is Int)
	// (The public-facing methods ensure the types are correct)
	int64_t times = args[2].integer;

	if (times < 0 || times > INT32_MAX)
	{
		VM_PushString(thread, strings::times);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	StringBuffer *buf = _SB(THISV);
	String *str = args[1].common.string;

	for (int32_t i = 0; i < (int32_t)times; i++)
		buf->Append(thread, str);

	VM_Push(thread, THISV);
}
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
		buf->Append(thread, 2, (uchar*)&pair);
	}
	else
		buf->Append(thread, 1, (uchar)codepoint);

	VM_Push(thread, THISV);
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_insertInternal)
{
	// insertInternal(index is Int, value is String)
	// (The public-facing methods ensure the types are correct)
	StringBuffer *buf = _SB(THISV);
	int64_t index = args[1].integer;

	if (index < 0 || index > buf->GetLength())
	{
		VM_PushString(thread, strings::index);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	buf->Insert(thread, (int32_t)index, args[2].common.string);

	VM_Push(thread, THISV);
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_clear)
{
	StringBuffer *buf = _SB(THISV);
	buf->Clear();
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_toString)
{
	StringBuffer *buf = _SB(THISV);
	VM_PushString(thread, buf->ToString(thread));
}

void aves_StringBuffer_finalize(void *basePtr)
{
	// NOTE: Do not delete the memory! Just call the destructor.
	// The GC allocated things, so we let it clean things up.
	((StringBuffer*)basePtr)->~StringBuffer();
}