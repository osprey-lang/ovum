#pragma once

#ifndef AVES__BUFFER_H
#define AVES__BUFFER_H

#include "aves.h"

typedef struct Buffer_S
{
	uint32_t size; // The total number of bytes in the buffer
	union
	{
		uint8_t *bytes;
		int8_t *sbytes;
		int16_t *int16s;
		int32_t *int32s;
		int64_t *int64s;
		uint16_t *uint16s;
		uint32_t *uint32s;
		uint64_t *uint64s;
		float *floats;
		double *doubles;
	};
} Buffer;

AVES_API void aves_Buffer_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Buffer_new);

AVES_API NATIVE_FUNCTION(aves_Buffer_get_size);

AVES_API NATIVE_FUNCTION(aves_Buffer_readByte);
AVES_API NATIVE_FUNCTION(aves_Buffer_readSbyte);
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt16);
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt32);
AVES_API NATIVE_FUNCTION(aves_Buffer_readInt64);
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt16);
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt32);
AVES_API NATIVE_FUNCTION(aves_Buffer_readUInt64);
AVES_API NATIVE_FUNCTION(aves_Buffer_readFloat32);
AVES_API NATIVE_FUNCTION(aves_Buffer_readFloat64);

AVES_API NATIVE_FUNCTION(aves_Buffer_writeByte);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeSByte);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt16);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt32);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeInt64);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt16);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt32);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeUInt64);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeFloat32);
AVES_API NATIVE_FUNCTION(aves_Buffer_writeFloat64);

void aves_Buffer_finalize(ThreadHandle thread, void *basePtr);


typedef struct BufferView_S
{
	// These must be the same as the values in Buffer.osp
	enum BufferViewKind
	{
		INT16 = 1,
		INT32 = 2,
		INT64 = 3,
		UINT16 = 4,
		UINT32 = 5,
		UINT64 = 6,
		FLOAT32 = 7,
		FLOAT64 = 8,
	};
	Value buffer;
	BufferViewKind kind;
} BufferView;

AVES_API void aves_BufferView_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_BufferView_new);

AVES_API NATIVE_FUNCTION(aves_BufferView_get_item);
AVES_API NATIVE_FUNCTION(aves_BufferView_set_item);

AVES_API NATIVE_FUNCTION(aves_BufferView_get_length);

AVES_API NATIVE_FUNCTION(aves_BufferView_get_buffer);
AVES_API NATIVE_FUNCTION(aves_BufferView_get_kind);

bool aves_BufferView_getReferences(void *basePtr, unsigned int &valc, Value **target);

#endif // AVES__BUFFER_H