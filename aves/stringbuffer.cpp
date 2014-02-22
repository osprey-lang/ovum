#include "ov_stringbuffer.h"
#include "aves_stringbuffer.h"
#include <new> // For placement new

AVES_API void aves_StringBuffer_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(StringBuffer));
	Type_SetReferenceGetter(type, aves_StringBuffer_getReferences);
	Type_SetFinalizer(type, aves_StringBuffer_finalize);
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_new)
{
	StringBuffer *buf = (StringBuffer*)THISV.instance;

	new(buf) StringBuffer(thread);
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_newCap)
{
	StringBuffer *buf = (StringBuffer*)THISV.instance;

	IntFromValue(thread, args + 1);
	int64_t capacity = args[1].integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	new(buf) StringBuffer(thread, capacity);
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_length)
{
	StringBuffer *buf = (StringBuffer*)THISV.instance;
	VM_PushInt(thread, buf->GetLength());
}
AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_capacity)
{
	StringBuffer *buf = (StringBuffer*)THISV.instance;
	VM_PushInt(thread, buf->GetCapacity());
}

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendLine)
{
	StringBuffer *buf = (StringBuffer*)THISV.instance;
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

	StringBuffer *buf = (StringBuffer*)THISV.instance;
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

	StringBuffer *buf = (StringBuffer*)THISV.instance;
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
	StringBuffer *buf = (StringBuffer*)THISV.instance;
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
AVES_API NATIVE_FUNCTION(aves_StringBuffer_toString)
{
	StringBuffer *buf = (StringBuffer*)THISV.instance;

	VM_PushString(thread, buf->ToString(thread));
}

bool aves_StringBuffer_getReferences(void *basePtr, unsigned int &valc, Value **target)
{
	*target = nullptr;
	valc = 0;
	return false;
}

void aves_StringBuffer_finalize(ThreadHandle thread, void *basePtr)
{
	// NOTE: Do not delete the memory! Just call the destructor.
	// The GC allocated things, so we let it clean things up.
	((StringBuffer*)basePtr)->~StringBuffer();
}