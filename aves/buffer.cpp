#include "aves_buffer.h"

TypeHandle BufferType;

AVES_API void aves_Buffer_init(TypeHandle type)
{
	BufferType = type;

	Type_SetInstanceSize(type, sizeof(Buffer));
	Type_SetFinalizer(type, aves_Buffer_finalize);
}

AVES_API NATIVE_FUNCTION(aves_Buffer_new)
{
	int64_t size64 = IntFromValue(thread, args[1]).integer;
	if (size64 < 0 || size64 > UINT32_MAX)
	{
		GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}

	Buffer *buf = (Buffer*)THISV.instance;

	buf->size = (uint32_t)size64;
	if (buf->size > 0)
	{
		buf->bytes = new uint8_t[buf->size];
		memset(buf->bytes, 0, buf->size);
	}
}

AVES_API NATIVE_FUNCTION(aves_Buffer_get_size)
{
	Buffer *buf = (Buffer*)THISV.instance;
	VM_PushInt(thread, buf->size);
}

unsigned int GetBufferIndex(ThreadHandle thread, Buffer *buf, Value indexValue, int valueSize)
{
	int64_t index64 = IntFromValue(thread, indexValue).integer;
	if (index64 < 0 || index64 >= buf->size / valueSize)
	{
		VM_PushString(thread, strings::index);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	return (unsigned int)index64;
}

AVES_API NATIVE_FUNCTION(aves_Buffer_readByte)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	VM_PushUInt(thread, buf->bytes[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readSbyte)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	VM_PushInt(thread, buf->sbytes[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt16)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	VM_PushInt(thread, buf->int16s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt32)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	VM_PushInt(thread, buf->int32s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt64)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	VM_PushInt(thread, buf->int64s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt16)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	VM_PushUInt(thread, buf->uint16s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt32)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	VM_PushUInt(thread, buf->uint32s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt64)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	VM_PushUInt(thread, buf->uint64s[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readFloat32)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	VM_PushReal(thread, buf->floats[index]);
}
AVES_API NATIVE_FUNCTION(aves_Buffer_readFloat64)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	VM_PushReal(thread, buf->doubles[index]);
}

AVES_API NATIVE_FUNCTION(aves_Buffer_writeByte)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->bytes[index] = (uint8_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeSByte)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 1);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->sbytes[index] = (int8_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt16)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->int16s[index] = (int16_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt32)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->int32s[index] = (int32_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt64)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	int64_t value = args[2].integer;

	buf->int64s[index] = value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt16)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 2);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->uint16s[index] = (uint16_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt32)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->uint32s[index] = (uint32_t)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt64)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	if (args[2].type != Types::Int && args[2].type != Types::UInt)
		VM_ThrowTypeError(thread);
	uint64_t value = args[2].uinteger;

	buf->uint64s[index] = value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeFloat32)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 4);
	if (args[2].type != Types::Real)
		VM_ThrowTypeError(thread);
	double value = args[2].real;

	buf->floats[index] = (float)value;
}
AVES_API NATIVE_FUNCTION(aves_Buffer_writeFloat64)
{
	Buffer *buf = (Buffer*)THISV.instance;
	unsigned int index = GetBufferIndex(thread, buf, args[1], 8);
	if (args[2].type != Types::Real)
		VM_ThrowTypeError(thread);
	double value = args[2].real;

	buf->doubles[index] = value;
}

void aves_Buffer_finalize(ThreadHandle thread, void *basePtr)
{
	Buffer *buf = reinterpret_cast<Buffer*>(basePtr);
	buf->size = 0;
	delete[] buf->bytes;
}


AVES_API void aves_BufferView_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(BufferView));
	Type_SetReferenceGetter(type, aves_BufferView_getReferences);
}

AVES_API NATIVE_FUNCTION(aves_BufferView_new)
{
	if (!IsType(args[1], BufferType))
		VM_ThrowTypeError(thread);
	if (!IsType(args[2], Types::BufferViewKind))
		VM_ThrowTypeError(thread);
	if (args[2].integer < BufferView::INT16 ||
		args[2].integer > BufferView::FLOAT64)
	{
		VM_PushString(thread, strings::kind);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	BufferView *view = (BufferView*)THISV.instance;
	view->buffer = args[1];
	view->kind = (BufferView::BufferViewKind)args[2].integer;
}

AVES_API NATIVE_FUNCTION(aves_BufferView_get_item)
{
	BufferView *view = (BufferView*)THISV.instance;
	Buffer *buf = (Buffer*)view->buffer.instance;

	unsigned int index;
	switch (view->kind)
	{
	case BufferView::INT16:
		index = GetBufferIndex(thread, buf, args[1], 2);
		VM_PushInt(thread, buf->int16s[index]);
		break;
	case BufferView::INT32:
		index = GetBufferIndex(thread, buf, args[1], 4);
		VM_PushInt(thread, buf->int32s[index]);
		break;
	case BufferView::INT64:
		index = GetBufferIndex(thread, buf, args[1], 8);
		VM_PushInt(thread, buf->int64s[index]);
		break;
	case BufferView::UINT16:
		index = GetBufferIndex(thread, buf, args[1], 2);
		VM_PushUInt(thread, buf->uint16s[index]);
		break;
	case BufferView::UINT32:
		index = GetBufferIndex(thread, buf, args[1], 4);
		VM_PushUInt(thread, buf->uint32s[index]);
		break;
	case BufferView::UINT64:
		index = GetBufferIndex(thread, buf, args[1], 8);
		VM_PushUInt(thread, buf->uint64s[index]);
		break;
	case BufferView::FLOAT32:
		index = GetBufferIndex(thread, buf, args[1], 4);
		VM_PushReal(thread, buf->floats[index]);
		break;
	case BufferView::FLOAT64:
		index = GetBufferIndex(thread, buf, args[1], 8);
		VM_PushReal(thread, buf->doubles[index]);
		break;
	}
}
AVES_API NATIVE_FUNCTION(aves_BufferView_set_item)
{
	BufferView *view = (BufferView*)THISV.instance;
	Buffer *buf = (Buffer*)view->buffer.instance;

	if (view->kind >= BufferView::INT16 && view->kind <= BufferView::UINT64)
	{
		if (args[2].type != Types::Int && args[2].type != Types::UInt)
			VM_ThrowTypeError(thread);
	}
	else if (args[2].type != Types::Real)
		VM_ThrowTypeError(thread);

	unsigned int index;
	switch (view->kind)
	{
	case BufferView::INT16:
		index = GetBufferIndex(thread, buf, args[1], 2);
		buf->int16s[index] = (int16_t)args[2].integer;
		break;
	case BufferView::INT32:
		index = GetBufferIndex(thread, buf, args[1], 4);
		buf->int32s[index] = (int32_t)args[2].integer;
		break;
	case BufferView::INT64:
		index = GetBufferIndex(thread, buf, args[1], 8);
		buf->int64s[index] = args[2].integer;
		break;
	case BufferView::UINT16:
		index = GetBufferIndex(thread, buf, args[1], 2);
		buf->uint16s[index] = (uint16_t)args[2].uinteger;
		break;
	case BufferView::UINT32:
		index = GetBufferIndex(thread, buf, args[1], 4);
		buf->uint32s[index] = (uint32_t)args[2].uinteger;
		break;
	case BufferView::UINT64:
		index = GetBufferIndex(thread, buf, args[1], 8);
		buf->uint64s[index] = args[2].uinteger;
		break;
	case BufferView::FLOAT32:
		index = GetBufferIndex(thread, buf, args[1], 4);
		buf->floats[index] = (float)args[2].real;
		break;
	case BufferView::FLOAT64:
		index = GetBufferIndex(thread, buf, args[1], 8);
		buf->doubles[index] = args[2].real;
		break;
	}
}

AVES_API NATIVE_FUNCTION(aves_BufferView_get_length)
{
	BufferView *view = (BufferView*)THISV.instance;
	Buffer *buf = (Buffer*)view->buffer.instance;

	int valueSize;
	switch (view->kind)
	{
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
	default:
		valueSize = -1; // wtf, this isn't supposed to happen
		break;
	}

	VM_PushInt(thread, buf->size / valueSize);
}

AVES_API NATIVE_FUNCTION(aves_BufferView_get_buffer)
{
	BufferView *view = (BufferView*)THISV.instance;
	VM_Push(thread, view->buffer);
}
AVES_API NATIVE_FUNCTION(aves_BufferView_get_kind)
{
	BufferView *view = (BufferView*)THISV.instance;
	Value kind;
	kind.type = Types::BufferViewKind;
	kind.integer = view->kind;
	VM_Push(thread, kind);
}

bool aves_BufferView_getReferences(void *basePtr, unsigned int &valc, Value **target)
{
	BufferView *view = (BufferView*)basePtr;
	valc = 1;
	*target = &view->buffer;
	return false;
}