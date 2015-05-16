#include "aves_buffer.h"
#include <new>

TypeHandle BufferType;

AVES_API void aves_Buffer_init(TypeHandle type)
{
	BufferType = type;

	Type_SetInstanceSize(type, sizeof(Buffer));
	Type_SetFinalizer(type, aves_Buffer_finalize);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_new)
{
	CHECKED(IntFromValue(thread, args + 1));
	int64_t size64 = args[1].v.integer;
	if (size64 < 0 || size64 > UINT32_MAX)
	{
		VM_PushString(thread, strings::size);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	Buffer *buf = THISV.Get<Buffer>();

	buf->size = (uint32_t)size64;
	if (buf->size > 0)
	{
		CHECKED_MEM(buf->bytes = new(std::nothrow) uint8_t[buf->size]);
		memset(buf->bytes, 0, buf->size);
	}
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Buffer_get_size)
{
	Buffer *buf = THISV.Get<Buffer>();
	VM_PushInt(thread, buf->size);
	RETURN_SUCCESS;
}

int GetBufferIndex(ThreadHandle thread, Buffer *buf, Value indexValue, int valueSize, unsigned int &index)
{
	int r;
	if ((r = IntFromValue(thread, &indexValue)) != OVUM_SUCCESS) return r;
	int64_t index64 = indexValue.v.integer;
	if (index64 < 0 || index64 >= buf->size / valueSize)
	{
		VM_PushString(thread, strings::index);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	index = (unsigned int)index64;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readByte)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	VM_PushUInt(thread, buf->bytes[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readSByte)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	VM_PushInt(thread, buf->sbytes[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readInt16)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	VM_PushInt(thread, buf->int16s[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readInt32)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	VM_PushInt(thread, buf->int32s[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readInt64)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	VM_PushInt(thread, buf->int64s[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readUInt16)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	VM_PushUInt(thread, buf->uint16s[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readUInt32)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	VM_PushUInt(thread, buf->uint32s[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readUInt64)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	VM_PushUInt(thread, buf->uint64s[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readFloat32)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	VM_PushReal(thread, buf->floats[index]);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readFloat64)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	VM_PushReal(thread, buf->doubles[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeByte)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->bytes[index] = (uint8_t)args[2].v.uinteger;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeSByte)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->sbytes[index] = (int8_t)args[2].v.integer;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeInt16)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->int16s[index] = (int16_t)args[2].v.integer;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeInt32)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->int32s[index] = (int32_t)args[2].v.integer;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeInt64)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->int64s[index] = args[2].v.integer;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeUInt16)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->uint16s[index] = (uint16_t)args[2].v.uinteger;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeUInt32)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->uint32s[index] = (uint32_t)args[2].v.uinteger;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeUInt64)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		return VM_ThrowTypeError(thread);

	buf->uint64s[index] = args[2].v.uinteger;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeFloat32)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	if (args[2].type != Types::Real)
		return VM_ThrowTypeError(thread);

	buf->floats[index] = (float)args[2].v.real;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeFloat64)
{
	Buffer *buf = THISV.Get<Buffer>();
	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	if (args[2].type != Types::Real)
		return VM_ThrowTypeError(thread);

	buf->doubles[index] = args[2].v.real;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Buffer_copyInternal)
{
	// copyInternal(source is Buffer, sourceIndex is Int, dest is Buffer, destIndex is Int, count is Int)
	// The public-facing method checks all the types and also
	// range-checks the indexes and count.
	Buffer *src = (Buffer*)args[0].v.instance;
	Buffer *dest = (Buffer*)args[2].v.instance;
	int32_t srcIndex = (int32_t)args[1].v.integer;
	int32_t destIndex = (int32_t)args[3].v.integer;
	int32_t count = (int32_t)args[4].v.integer;

	// Copying the data could take a while if there's a lot to copy,
	// so let's enter an unmanaged region during the copying, so we
	// don't block the GC if it decides to run.
	// Note: use memmove to make it safe for src and dest to overlap.
	VM_EnterUnmanagedRegion(thread);
	memmove(dest->bytes + destIndex, src->bytes + srcIndex, count);
	VM_LeaveUnmanagedRegion(thread);

	RETURN_SUCCESS;
}

void aves_Buffer_finalize(void *basePtr)
{
	Buffer *buf = reinterpret_cast<Buffer*>(basePtr);
	buf->size = 0;
	free(buf->bytes);
}

AVES_API uint8_t *aves_Buffer_getDataPointer(Value *buffer, uint32_t *bufferSize)
{
	if (buffer == nullptr || !IsType(buffer, BufferType))
		return nullptr;

	Buffer *buf = buffer->Get<Buffer>();
	if (bufferSize != nullptr)
		*bufferSize = buf->size;

	return buf->bytes;
}


AVES_API void aves_BufferView_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(BufferView));

	Type_AddNativeField(type, offsetof(BufferView, buffer), NativeFieldType::VALUE);
}

AVES_API NATIVE_FUNCTION(aves_BufferView_new)
{
	if (IS_NULL(args[1]))
		return VM_ThrowErrorOfType(thread, Types::ArgumentNullError, 0);
	if (!IsType(args + 1, BufferType))
		return VM_ThrowTypeError(thread);
	if (!IsType(args + 2, Types::BufferViewKind))
		return VM_ThrowTypeError(thread);
	if (args[2].v.integer < BufferView::BYTE ||
		args[2].v.integer > BufferView::FLOAT64)
	{
		VM_PushString(thread, strings::kind);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	BufferView *view = THISV.Get<BufferView>();
	view->buffer = args[1];
	view->kind = (BufferView::BufferViewKind)args[2].v.integer;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_BufferView_get_item)
{
	BufferView *view = THISV.Get<BufferView>();
	Buffer *buf = view->buffer.Get<Buffer>();

	int valueSize = -1;
	switch (view->kind)
	{
	case BufferView::BYTE:
	case BufferView::SBYTE:
		valueSize = 1;
		break;
	case BufferView::INT16:
	case BufferView::UINT16:
		valueSize = 2;
		break;
	case BufferView::INT32:
	case BufferView::UINT32:
	case BufferView::FLOAT32:
		valueSize = 4;
		break;
	case BufferView::INT64:
	case BufferView::UINT64:
	case BufferView::FLOAT64:
		valueSize = 8;
		break;
	}

	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], valueSize, index));

	switch (view->kind)
	{
	case BufferView::BYTE:
		VM_PushUInt(thread, buf->bytes[index]);
		break;
	case BufferView::SBYTE:
		VM_PushInt(thread, buf->sbytes[index]);
		break;
	case BufferView::INT16:
		VM_PushInt(thread, buf->int16s[index]);
		break;
	case BufferView::INT32:
		VM_PushInt(thread, buf->int32s[index]);
		break;
	case BufferView::INT64:
		VM_PushInt(thread, buf->int64s[index]);
		break;
	case BufferView::UINT16:
		VM_PushUInt(thread, buf->uint16s[index]);
		break;
	case BufferView::UINT32:
		VM_PushUInt(thread, buf->uint32s[index]);
		break;
	case BufferView::UINT64:
		VM_PushUInt(thread, buf->uint64s[index]);
		break;
	case BufferView::FLOAT32:
		VM_PushReal(thread, buf->floats[index]);
		break;
	case BufferView::FLOAT64:
		VM_PushReal(thread, buf->doubles[index]);
		break;
	}
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_BufferView_set_item)
{
	BufferView *view = THISV.Get<BufferView>();
	Buffer *buf = view->buffer.Get<Buffer>();

	if (view->kind >= BufferView::BYTE && view->kind <= BufferView::UINT64)
	{
		if (args[2].type != Types::Int && args[2].type != Types::UInt)
			return VM_ThrowTypeError(thread);
	}
	else if (args[2].type != Types::Real)
		return VM_ThrowTypeError(thread);

	int valueSize = -1;
	switch (view->kind)
	{
	case BufferView::BYTE:
	case BufferView::SBYTE:
		valueSize = 1;
		break;
	case BufferView::INT16:
	case BufferView::UINT16:
		valueSize = 2;
		break;
	case BufferView::INT32:
	case BufferView::UINT32:
	case BufferView::FLOAT32:
		valueSize = 4;
		break;
	case BufferView::INT64:
	case BufferView::UINT64:
	case BufferView::FLOAT64:
		valueSize = 8;
		break;
	}

	unsigned int index;
	CHECKED(GetBufferIndex(thread, buf, args[1], valueSize, index));

	switch (view->kind)
	{
	case BufferView::BYTE:
		buf->bytes[index] = (uint8_t)args[2].v.integer;
		break;
	case BufferView::SBYTE:
		buf->sbytes[index] = (int8_t)args[2].v.integer;
		break;
	case BufferView::INT16:
		buf->int16s[index] = (int16_t)args[2].v.integer;
		break;
	case BufferView::INT32:
		buf->int32s[index] = (int32_t)args[2].v.integer;
		break;
	case BufferView::INT64:
		buf->int64s[index] = args[2].v.integer;
		break;
	case BufferView::UINT16:
		buf->uint16s[index] = (uint16_t)args[2].v.uinteger;
		break;
	case BufferView::UINT32:
		buf->uint32s[index] = (uint32_t)args[2].v.uinteger;
		break;
	case BufferView::UINT64:
		buf->uint64s[index] = args[2].v.uinteger;
		break;
	case BufferView::FLOAT32:
		buf->floats[index] = (float)args[2].v.real;
		break;
	case BufferView::FLOAT64:
		buf->doubles[index] = args[2].v.real;
		break;
	}
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_BufferView_get_length)
{
	BufferView *view = THISV.Get<BufferView>();
	Buffer *buf = view->buffer.Get<Buffer>();

	int valueSize = -1;
	switch (view->kind)
	{
	case BufferView::BYTE:
	case BufferView::SBYTE:
		valueSize = 1;
		break;
	case BufferView::INT16:
	case BufferView::UINT16:
		valueSize = 2;
		break;
	case BufferView::INT32:
	case BufferView::UINT32:
	case BufferView::FLOAT32:
		valueSize = 4;
		break;
	case BufferView::INT64:
	case BufferView::UINT64:
	case BufferView::FLOAT64:
		valueSize = 8;
		break;
	}

	VM_PushInt(thread, buf->size / valueSize);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_BufferView_get_buffer)
{
	BufferView *view = THISV.Get<BufferView>();
	VM_Push(thread, &view->buffer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_BufferView_get_kind)
{
	BufferView *view = THISV.Get<BufferView>();
	Value kind;
	kind.type = Types::BufferViewKind;
	kind.v.integer = view->kind;
	VM_Push(thread, &kind);
	RETURN_SUCCESS;
}