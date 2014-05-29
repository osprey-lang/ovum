#pragma once

#ifndef VM__MODULEREADER_INTERNAL_H
#define VM__MODULEREADER_INTERNAL_H

#include <string>
#include "ov_vm.internal.h"
#include "pathname.internal.h"

enum class SeekOrigin
{
	BEGIN = 0,
	CURRENT = 1,
	END = 2,
};

class ModuleReader
{
private:
	static const size_t BUFFER_SIZE = 65536;

	HANDLE stream;

	// The file position of the first byte in the buffer
	uint32_t bufferPosition;
	// The current position within our buffer
	uint32_t bufferIndex;
	// The number of bytes read into our buffer in the last
	// read operation
	uint32_t bufferDataSize;
	// Note: do not make this a fixed array. We don't want to
	// fill up the call stack too much.
	uint8_t *buffer;

	// Gets the real position of the file pointer
	unsigned long GetFilePosition();
	void FillBuffer();
	uint32_t ReadRaw(void *dest, uint32_t count);

public:
	PathName fileName;

	ModuleReader();
	~ModuleReader();

	void Open(const wchar_t *fileName);
	void Open(const PathName &fileName);

	void Read(void *dest, uint32_t count);

	unsigned long GetPosition();

	void Seek(long amount, SeekOrigin origin);

	inline int8_t ReadInt8()
	{
		int8_t target;
		Read(&target, sizeof(int8_t));
		return target;
	}
	inline uint8_t ReadUInt8()
	{
		uint8_t target;
		Read(&target, sizeof(uint8_t));
		return target;
	}

	// All the reading functions below assume the system is little-endian.
	// This assumption will be fixed at an unspecified later date.

	inline int16_t ReadInt16()
	{
		int16_t target;
		Read(&target, sizeof(int16_t));
		return target;
	}
	inline uint16_t ReadUInt16()
	{
		uint16_t target;
		Read(&target, sizeof(uint16_t));
		return target;
	}

	inline int32_t ReadInt32()
	{
		int32_t target;
		Read(&target, sizeof(int32_t));
		return target;
	}
	inline uint32_t ReadUInt32()
	{
		uint32_t target;
		Read(&target, sizeof(uint32_t));
		return target;
	}

	inline int64_t ReadInt64()
	{
		int64_t target;
		Read(&target, sizeof(int64_t));
		return target;
	}
	inline uint64_t ReadUInt64()
	{
		uint64_t target;
		Read(&target, sizeof(uint64_t));
		return target;
	}

	inline TokenId ReadToken()
	{
		TokenId target;
		Read(&target, sizeof(TokenId));
		return target;
	}

	inline void SkipCollection()
	{
		using namespace std;
		uint32_t size = ReadUInt32();
		Seek(size, SeekOrigin::CURRENT);
	}

	String *ReadString();
	String *ReadStringOrNull();
	char *ReadCString();

private:
	String *ReadShortString(const int32_t length);
	String *ReadLongString(const int32_t length);

	void HandleError(DWORD error);

	static const int MaxShortStringLength = 128;
};

#endif // VM__MODULEREADER_INTERNAL_H