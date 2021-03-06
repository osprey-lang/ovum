#include "buffer.h"
#include "../aves_state.h"
#include <new>

using namespace aves;

AVES_API int aves_Buffer_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Buffer));
	Type_SetFinalizer(type, aves_Buffer_finalize);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_new)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));
	int64_t size64 = args[1].v.integer;
	if (size64 < 0 || size64 > OVUM_ISIZE_MAX)
	{
		VM_PushString(thread, strings::size);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	Buffer *buf = THISV.Get<Buffer>();

	buf->size = (size_t)size64;
	if (buf->size > 0)
	{
		CHECKED_MEM(buf->bytes = new(std::nothrow) uint8_t[buf->size]);
		// If the size is particularly large, the zeroing operation may take a
		// large amount of time. We enter an unmanaged region so the GC can run
		// if it really needs to.
		VM_EnterUnmanagedRegion(thread);
		memset(buf->bytes, 0, buf->size);
		VM_LeaveUnmanagedRegion(thread);
	}
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Buffer_get_size)
{
	Buffer *buf = THISV.Get<Buffer>();
	VM_PushInt(thread, (int64_t)buf->size);
	RETURN_SUCCESS;
}

int GetBufferIndex(
	ThreadHandle thread,
	Buffer *buf,
	Value indexValue,
	size_t valueSize,
	size_t &index
)
{
	Aves *aves = Aves::Get(thread);

	int r;
	if ((r = IntFromValue(thread, &indexValue)) != OVUM_SUCCESS)
		return r;
	int64_t index64 = indexValue.v.integer;
	if (index64 < 0 || index64 >= buf->size / valueSize)
	{
		VM_PushString(thread, strings::index);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	index = (size_t)index64;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readByte)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	VM_PushUInt(thread, buf->bytes[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readSByte)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	VM_PushInt(thread, buf->sbytes[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readInt16)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	VM_PushInt(thread, buf->int16s[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readInt32)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	VM_PushInt(thread, buf->int32s[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readInt64)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	VM_PushInt(thread, buf->int64s[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readUInt16)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	VM_PushUInt(thread, buf->uint16s[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readUInt32)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	VM_PushUInt(thread, buf->uint32s[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readUInt64)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	VM_PushUInt(thread, buf->uint64s[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readFloat32)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	VM_PushReal(thread, buf->floats[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_readFloat64)
{
	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	VM_PushReal(thread, buf->doubles[index]);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeByte)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->bytes[index] = (uint8_t)args[2].v.uinteger;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeSByte)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 1, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->sbytes[index] = (int8_t)args[2].v.integer;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeInt16)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->int16s[index] = (int16_t)args[2].v.integer;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeInt32)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->int32s[index] = (int32_t)args[2].v.integer;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeInt64)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->int64s[index] = args[2].v.integer;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeUInt16)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 2, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->uint16s[index] = (uint16_t)args[2].v.uinteger;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeUInt32)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->uint32s[index] = (uint32_t)args[2].v.uinteger;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeUInt64)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	if (args[2].type != aves->aves.Int && args[2].type != aves->aves.UInt)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->uint64s[index] = args[2].v.uinteger;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeFloat32)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 4, index));

	if (args[2].type != aves->aves.Real)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	buf->floats[index] = (float)args[2].v.real;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Buffer_writeFloat64)
{
	Aves *aves = Aves::Get(thread);

	Buffer *buf = THISV.Get<Buffer>();
	size_t index;
	CHECKED(GetBufferIndex(thread, buf, args[1], 8, index));

	if (args[2].type != aves->aves.Real)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

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
	size_t srcIndex = (size_t)args[1].v.integer;
	size_t destIndex = (size_t)args[3].v.integer;
	size_t count = (size_t)args[4].v.integer;

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

AVES_API uint8_t *aves_Buffer_getDataPointer(Value *buffer, size_t *bufferSize)
{
	if (buffer == nullptr)
		return nullptr;

	Buffer *buf = buffer->Get<Buffer>();
	if (bufferSize != nullptr)
		*bufferSize = buf->size;

	return buf->bytes;
}


AVES_API int aves_BufferView_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(BufferView));

	int r;
	r = Type_AddNativeField(type, offsetof(BufferView, buffer), NativeFieldType::VALUE);
	if (r != OVUM_SUCCESS)
		return r;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_BufferView_new)
{
	Aves *aves = Aves::Get(thread);

	if (IS_NULL(args[1]))
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentNullError, 0);
	if (!IsType(args + 1, aves->aves.Buffer))
	{
		VM_PushString(thread, strings::buffer); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}
	if (!IsType(args + 2, aves->aves.BufferViewKind))
	{
		VM_PushString(thread, strings::kind); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}
	if (args[2].v.integer < BufferView::BYTE ||
		args[2].v.integer > BufferView::FLOAT64)
	{
		VM_PushString(thread, strings::kind);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
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

	size_t valueSize = 0;
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

	size_t index;
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
	Aves *aves = Aves::Get(thread);

	BufferView *view = THISV.Get<BufferView>();
	Buffer *buf = view->buffer.Get<Buffer>();

	bool isValueRightType = view->kind >= BufferView::BYTE && view->kind <= BufferView::UINT64
		? args[2].type == aves->aves.Int || args[2].type == aves->aves.UInt
		: args[2].type == aves->aves.Real;
	if (!isValueRightType)
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	size_t valueSize = 0;
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

	size_t index;
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

	size_t valueSize = 0;
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
	Aves *aves = Aves::Get(thread);

	BufferView *view = THISV.Get<BufferView>();
	Value kind;
	kind.type = aves->aves.BufferViewKind;
	kind.v.integer = view->kind;
	VM_Push(thread, &kind);
	RETURN_SUCCESS;
}
