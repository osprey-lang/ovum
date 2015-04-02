#include <ov_stringbuffer.h>
#include <ov_unicode.h>
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
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	if (!buf->Init((int32_t)capacity))
		return VM_ThrowMemoryError(thread);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_get_item)
{
	StringBuffer *buf = _SB(THISV);
	CHECKED(IntFromValue(thread, args + 1));

	if (args[1].integer < 0 || args[1].integer >= buf->GetLength())
	{
		VM_PushString(thread, strings::index); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}
	int32_t index = (int32_t)args[1].integer;

	Value character;
	character.type = Types::Char;
	character.uinteger = buf->GetDataPointer()[index];
	VM_Push(thread, &character);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_set_item)
{
	StringBuffer *buf = _SB(THISV);
	CHECKED(IntFromValue(thread, args + 1));
	if (args[2].type != Types::Char)
		return VM_ThrowTypeError(thread);

	if (args[1].integer < 0 || args[1].integer >= buf->GetLength())
	{
		VM_PushString(thread, strings::index); // paramName
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}
	if (args[2].uinteger > 0xFFFF)
	{
		VM_PushString(thread, strings::value);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	int32_t index = (int32_t)args[1].integer;
	buf->GetDataPointer()[index] = (uchar)args[2].uinteger;
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

AVES_API NATIVE_FUNCTION(aves_StringBuffer_append)
{
	// append(value is String, times is Int)
	int64_t times = args[2].integer;

	if (times < 0 || times > INT32_MAX)
	{
		VM_PushString(thread, strings::times);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	StringBuffer *buf = _SB(THISV);
	String *str = args[1].common.string;

	for (int32_t i = 0; i < (int32_t)times; i++)
		if (!buf->Append(str))
			return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendLine)
{
	StringBuffer *buf = _SB(THISV);
	if (!buf->Append(strings::newline))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendCodepoint)
{
	// appendCodepoint(cp is Int)
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

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendSubstrFromString)
{
	// appendSubstrFromString(str is String, index is Int, count is Int)
	// The public-facing method also range-checks the values

	StringBuffer *buf = _SB(THISV);
	String *str   = args[1].common.string;
	int32_t index = (int32_t)args[2].integer;
	int32_t count = (int32_t)args[3].integer;

	if (!buf->Append(count, &str->firstChar + index))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendSubstrFromBuffer)
{
	// appendSubstrFromBuffer(sb is StringBuffer, index is Int, count is Int)
	// The public-facing method also range-checks the values

	StringBuffer *dest = _SB(THISV);
	StringBuffer *src = _SB(args[1]);
	int32_t index = (int32_t)args[2].integer;
	int32_t count = (int32_t)args[3].integer;

	if (!dest->Append(count, src->GetDataPointer() + index))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_insert)
{
	// insert(index is Int, value is String)
	// The public-facing method also range-checks the values.
	StringBuffer *buf = _SB(THISV);
	int32_t index = (int32_t)args[1].integer;

	if (!buf->Insert(index, args[2].common.string))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_clear)
{
	StringBuffer *buf = _SB(THISV);
	buf->Clear();
	VM_Push(thread, THISP);
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

AVES_API NATIVE_FUNCTION(aves_StringBuffer_toStringSubstr)
{
	// toStringSubstr(start is Int, count is Int)
	// The public-facing method range-checks the values.

	StringBuffer *buf = _SB(THISV);
	int32_t start = (int32_t)args[1].integer;
	int32_t count = (int32_t)args[2].integer;

	String *result = GC_ConstructString(thread, count, buf->GetDataPointer() + start);
	if (!result)
		return VM_ThrowMemoryError(thread);
	VM_PushString(thread, result);
	RETURN_SUCCESS;
}

void aves_StringBuffer_finalize(void *basePtr)
{
	// NOTE: Do not delete the memory! Just call the destructor.
	// The GC allocated things, so we let it clean things up.
	StringBuffer *buf = (StringBuffer*)basePtr;
	buf->~StringBuffer();
}