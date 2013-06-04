#include "aves_buffer.h"

uint32_t BufferOffset;

#define _Buf(value) reinterpret_cast<::Buffer*>((value).instance + ::BufferOffset)

AVES_API void aves_Buffer_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Buffer));
	BufferOffset = Type_GetFieldOffset(type);

	Type_SetFinalizer(type, aves_Buffer_finalize);
}

AVES_API NATIVE_FUNCTION(aves_Buffer_new)
{
	int64_t size64 = IntFromValue(thread, args[1]).integer;
	if (size64 < 0 || size64 > UINT32_MAX)
	{
		GC_Construct(thread, ArgumentRangeError, 0, NULL);
		VM_Throw(thread);
	}

	Buffer *buf = reinterpret_cast<Buffer*>(THISV.instance + BufferOffset);

	buf->size = (uint32_t)size64;
	if (buf->size > 0)
		buf->bytes = new uint8_t[buf->size];
}

AVES_API NATIVE_FUNCTION(aves_Buffer_get_size)
{
	Buffer *buf = reinterpret_cast<Buffer*>(THISV.instance + BufferOffset);
	VM_PushInt(thread, buf->size);
}

unsigned int GetBufferIndex(ThreadHandle thread, Buffer *buf, Value indexValue, int valueSize)
{
	int64_t index64 = IntFromValue(thread, indexValue).integer;
	if (index64 < 0 || index64 >= buf->size / valueSize)
	{
		VM_PushString(thread, strings::index);
		GC_Construct(thread, ArgumentRangeError, 1, NULL);
		VM_Throw(thread);
	}

	return (unsigned int)index64;
}

AVES_API NATIVE_FUNCTION(aves_Buffer_readByte)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	VM_PushUInt(thread, buf->bytes[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readSbyte)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	VM_PushInt(thread, buf->sbytes[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt16)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	VM_PushInt(thread, buf->int16s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt32)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	VM_PushInt(thread, buf->int32s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt64)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	VM_PushInt(thread, buf->int64s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt16)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	VM_PushUInt(thread, buf->uint16s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt32)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	VM_PushUInt(thread, buf->uint32s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt64)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	VM_PushUInt(thread, buf->uint64s[index]);
}

AVES_API NATIVE_FUNCTION(aves_Buffer_writeByte)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->bytes[index] = (uint8_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeSByte)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->sbytes[index] = (int8_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt16)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->int16s[index] = (int16_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt32)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->int32s[index] = (int32_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt64)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->int64s[index] = value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt16)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->uint16s[index] = (uint16_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt32)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->uint32s[index] = (uint32_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt64)
{
	Buffer *buf = _Buf(THISV);
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	if (!IsInt(args + 2) && !IsUInt(args + 2))
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->uint64s[index] = value;
}

void aves_Buffer_finalize(ThreadHandle thread, void *basePtr)
{
	Buffer *buf = reinterpret_cast<Buffer*>(basePtr);
	buf->size = 0;
	delete[] buf->bytes;
}