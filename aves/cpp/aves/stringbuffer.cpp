#include <ovum_stringbuffer.h>
#include <ovum_unicode.h>
#include "stringbuffer.h"
#include "../aves_state.h"

using namespace aves;

AVES_API int aves_StringBuffer_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(StringBuffer));
	Type_SetFinalizer(type, aves_StringBuffer_finalize);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_new)
{
	StringBuffer *buf = THISV.Get<StringBuffer>();

	if (!buf->Init())
		return VM_ThrowMemoryError(thread);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_newCap)
{
	Aves *aves = Aves::Get(thread);

	StringBuffer *buf = THISV.Get<StringBuffer>();

	CHECKED(IntFromValue(thread, args + 1));
	int64_t capacity = args[1].v.integer;
	if (capacity < 0 || capacity > OVUM_ISIZE_MAX)
	{
		VM_PushString(thread, strings::capacity);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	if (!buf->Init((size_t)capacity))
		return VM_ThrowMemoryError(thread);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_get_item)
{
	Aves *aves = Aves::Get(thread);

	StringBuffer *buf = THISV.Get<StringBuffer>();
	CHECKED(IntFromValue(thread, args + 1));

	if (args[1].v.integer < 0 || args[1].v.integer >= buf->GetLength())
	{
		VM_PushString(thread, strings::index); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}
	size_t index = (size_t)args[1].v.integer;

	Value character;
	character.type = aves->aves.Char;
	character.v.uinteger = buf->GetDataPointer()[index];
	VM_Push(thread, &character);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_StringBuffer_set_item)
{
	Aves *aves = Aves::Get(thread);

	StringBuffer *buf = THISV.Get<StringBuffer>();
	CHECKED(IntFromValue(thread, args + 1));
	if (args[2].type != aves->aves.Char)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	if (args[1].v.integer < 0 || args[1].v.integer >= buf->GetLength())
	{
		VM_PushString(thread, strings::index); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}
	if (args[2].v.uinteger > 0xFFFF)
	{
		VM_PushString(thread, strings::value);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	size_t index = (size_t)args[1].v.integer;
	buf->GetDataPointer()[index] = (ovchar_t)args[2].v.uinteger;
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_length)
{
	StringBuffer *buf = THISV.Get<StringBuffer>();
	VM_PushInt(thread, (int64_t)buf->GetLength());
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_capacity)
{
	StringBuffer *buf = THISV.Get<StringBuffer>();
	VM_PushInt(thread, (int64_t)buf->GetCapacity());
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_append)
{
	Aves *aves = Aves::Get(thread);

	// append(value is String, times is Int)
	int64_t times = args[2].v.integer;

	if (times < 0 || times > OVUM_ISIZE_MAX)
	{
		VM_PushString(thread, strings::times);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	StringBuffer *buf = THISV.Get<StringBuffer>();
	String *str = args[1].v.string;

	for (size_t i = 0; i < (size_t)times; i++)
		if (!buf->Append(str))
			return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendLine)
{
	StringBuffer *buf = THISV.Get<StringBuffer>();
	if (!buf->Append(strings::newline))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendCodePoint)
{
	// appendCodepoint(cp: Int)
	// The public-facing method makes sure the type is right,
	// and also range-checks the value

	StringBuffer *buf = THISV.Get<StringBuffer>();
	ovwchar_t codepoint = (ovwchar_t)args[1].v.integer;

	if (codepoint > 0xFFFF)
	{
		// Surrogate pair, whoo!
		SurrogatePair pair = UC_ToSurrogatePair(codepoint);
		if (!buf->Append(2, (ovchar_t*)&pair))
			return VM_ThrowMemoryError(thread);
	}
	else
		if (!buf->Append(1, (ovchar_t)codepoint))
			return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendSubstringFromString)
{
	// appendSubstrFromString(str: String, index: Int, count: Int)
	// The public-facing method also range-checks the values

	StringBuffer *buf = THISV.Get<StringBuffer>();
	String *str = args[1].v.string;
	size_t index = (size_t)args[2].v.integer;
	size_t count = (size_t)args[3].v.integer;

	if (!buf->Append(count, &str->firstChar + index))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendSubstringFromBuffer)
{
	// appendSubstrFromBuffer(sb: StringBuffer, index: Int, count: Int)
	// The public-facing method also range-checks the values

	StringBuffer *dest = THISV.Get<StringBuffer>();
	StringBuffer *src = args[1].Get<StringBuffer>();
	size_t index = (size_t)args[2].v.integer;
	size_t count = (size_t)args[3].v.integer;

	if (!dest->Append(count, src->GetDataPointer() + index))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_insert)
{
	// insert(index: Int, value: String)
	// The public-facing method also range-checks the values.
	StringBuffer *buf = THISV.Get<StringBuffer>();
	size_t index = (size_t)args[1].v.integer;

	if (!buf->Insert(index, args[2].v.string))
		return VM_ThrowMemoryError(thread);

	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_clear)
{
	StringBuffer *buf = THISV.Get<StringBuffer>();
	buf->Clear();
	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_toString)
{
	String *result = THISV.Get<StringBuffer>()->ToString(thread);
	if (!result)
		return VM_ThrowMemoryError(thread);
	VM_PushString(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_toStringSubstring)
{
	// toStringSubstr(start is Int, count is Int)
	// The public-facing method range-checks the values.

	StringBuffer *buf = THISV.Get<StringBuffer>();
	size_t start = (size_t)args[1].v.integer;
	size_t count = (size_t)args[2].v.integer;

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
