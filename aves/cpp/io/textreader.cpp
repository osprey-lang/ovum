#include "textreader.h"
#include "../aves/buffer.h"
#include <ov_stringbuffer.h>
#include <stddef.h>

namespace strings
{
	LitString<10> _FillBufferName = { 10, 0, StringFlags::STATIC, 'f','i','l','l','B','u','f','f','e','r',0 };
}

MethodHandle TextReaderInst::FillBuffer;
String *TextReaderInst::FillBufferName = strings::_FillBufferName.AsString();

#define _TR(val)     reinterpret_cast<TextReaderInst*>((val).instance)

AVES_API int OVUM_CDECL io_TextReader_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(TextReaderInst));

	int status__;
	CHECKED(Type_AddNativeField(type, offsetof(TextReaderInst,stream), NativeFieldType::VALUE));
	CHECKED(Type_AddNativeField(type, offsetof(TextReaderInst,encoding), NativeFieldType::VALUE));
	CHECKED(Type_AddNativeField(type, offsetof(TextReaderInst,decoder), NativeFieldType::VALUE));
	CHECKED(Type_AddNativeField(type, offsetof(TextReaderInst,byteBuffer), NativeFieldType::VALUE));
	CHECKED(Type_AddNativeField(type, offsetof(TextReaderInst,charBuffer), NativeFieldType::VALUE));

	TextReaderInst::FillBuffer = Member_ToMethod(Type_GetMember(type, TextReaderInst::FillBufferName));
	RETURN_SUCCESS;

retStatus__:
	return status__;
}

AVES_API NATIVE_FUNCTION(io_TextReader_get_stream)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_Push(thread, &tr->stream);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(io_TextReader_set_stream)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->stream = args[1];
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_TextReader_get_encoding)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_Push(thread, &tr->encoding);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(io_TextReader_set_encoding)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->encoding = args[1];
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_TextReader_get_keepOpen)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_PushBool(thread, tr->keepOpen);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(io_TextReader_set_keepOpen)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->keepOpen = !!args[1].v.integer;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_TextReader_get_decoder)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_Push(thread, &tr->decoder);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(io_TextReader_set_decoder)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->decoder = args[1];
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_TextReader_get_byteBuffer)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_Push(thread, &tr->byteBuffer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(io_TextReader_set_byteBuffer)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->byteBuffer = args[1];
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_TextReader_get_charBuffer)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_Push(thread, &tr->charBuffer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(io_TextReader_set_charBuffer)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->charBuffer = args[1];
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_TextReader_get_charCount)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_PushInt(thread, tr->charCount);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(io_TextReader_set_charCount)
{
	CHECKED(IntFromValue(thread, args + 1));
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->charCount = (int32_t)args[1].v.integer;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(io_TextReader_get_charOffset)
{
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	VM_PushInt(thread, tr->charOffset);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(io_TextReader_set_charOffset)
{
	CHECKED(IntFromValue(thread, args + 1));
	TextReaderInst *tr = THISV.Get<TextReaderInst>();
	tr->charOffset = (int32_t)args[1].v.integer;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_TextReader_readLine)
{
	Pinned reader(THISP);
	TextReaderInst *tr = THISV.Get<TextReaderInst>();

	if (tr->charOffset == tr->charCount)
	{
		// If we are at the end, try to fill the buffer.
		// If charCount is 0 after this call, we're at
		// EOF, so return null.
		Value ignore;
		VM_Push(thread, THISP);
		CHECKED(VM_InvokeMethod(thread, TextReaderInst::FillBuffer, 0, &ignore));
		if (tr->charCount == 0)
		{
			VM_PushNull(thread);
			RETURN_SUCCESS;
		}
	}

	Pinned charBuffer(&tr->charBuffer);
	StringBuffer *cb = reinterpret_cast<StringBuffer*>(tr->charBuffer.v.instance);

	StringBuffer sb; // Initialize on demand only
	do
	{
		int32_t i = tr->charOffset;
		do
		{
			ovchar_t ch = cb->GetDataPointer()[i];
			if (ch == '\n' || ch == '\r')
			{
				// We found a line ending! Make sure a string is
				// stored in sb.
				int32_t length = i - tr->charOffset;
				if (sb.GetDataPointer() == nullptr)
					CHECKED_MEM(sb.Init(length));

				CHECKED_MEM(sb.Append(length, cb->GetDataPointer() + tr->charOffset));
				tr->charOffset = i + 1;
				// See if we have \r\n, and if so, skip the \n as well
				if (ch == '\r')
				{
					// The below is basically a translation of
					//   charOffset < charCount or fillBuffer() > 0
					// Note that fillBuffer sets charOffset to zero, so we
					// can do this:
					if (tr->charOffset == tr->charCount)
					{
						Value ignore;
						VM_Push(thread, THISP);
						CHECKED(VM_InvokeMethod(thread, TextReaderInst::FillBuffer, 0, &ignore));
					}
					// If fillBuffer was called, tr->charOffset and tr->charCount
					// will both have been updated.
					if (tr->charOffset < tr->charCount)
					{
						if (cb->GetDataPointer()[tr->charOffset] == '\n')
							tr->charOffset++;
					}
				}

				// sb now contains the result! Let's return it.
				goto returnResult;
			}
			i++;
		} while (i < tr->charCount);

		// Length of current data
		i = tr->charCount - tr->charOffset;
		if (sb.GetDataPointer() == nullptr)
			CHECKED_MEM(sb.Init(i + 128));
		CHECKED_MEM(sb.Append(i, cb->GetDataPointer() + tr->charOffset));

		// Refill buffer
		Value ignore;
		VM_Push(thread, THISP);
		CHECKED(VM_InvokeMethod(thread, TextReaderInst::FillBuffer, 0, &ignore));
	} while (tr->charCount > 0);

returnResult:
	String *result;
	CHECKED_MEM(result = sb.ToString(thread));
	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION
