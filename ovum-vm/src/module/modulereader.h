#pragma once

#include "../vm.h"
#include "../ee/vm.h"
#include "../util/pathname.h"

namespace ovum
{

class ModuleReader
{
private:
	static const size_t BUFFER_SIZE = 65536;

	os::FileHandle stream;

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
	// The file name from which the ModuleReader was created.
	PathName fileName;
	// The VM instance that the reader reads module data for.
	VM *vm;

	// Gets the real position of the file pointer
	unsigned long GetFilePosition();
	void FillBuffer();
	uint32_t ReadRaw(void *dest, uint32_t count);

public:
	ModuleReader(VM *owner);
	~ModuleReader();

	void Open(const pathchar_t *fileName);
	void Open(const PathName &fileName);

	inline const PathName &GetFileName() const
	{
		return fileName;
	}

	void Read(void *dest, uint32_t count);

	template<typename T>
	inline T Read()
	{
		T value;
		Read(&value, sizeof(T));
		return value;
	}

	unsigned long GetPosition();

	void Seek(long amount, os::SeekOrigin origin);

	inline int8_t ReadInt8()
	{
		return Read<int8_t>();
	}
	inline uint8_t ReadUInt8()
	{
		return Read<uint8_t>();
	}

	// All the reading functions below assume the system is little-endian.
	// This assumption will be fixed at an unspecified later date.

	inline int16_t ReadInt16()
	{
		return Read<int16_t>();
	}
	inline uint16_t ReadUInt16()
	{
		return Read<uint16_t>();
	}

	inline int32_t ReadInt32()
	{
		return Read<int32_t>();
	}
	inline uint32_t ReadUInt32()
	{
		return Read<uint32_t>();
	}

	inline int64_t ReadInt64()
	{
		return Read<int64_t>();
	}
	inline uint64_t ReadUInt64()
	{
		return Read<uint64_t>();
	}

	inline TokenId ReadToken()
	{
		return Read<TokenId>();
	}

	inline void SkipCollection()
	{
		using namespace std;
		uint32_t size = ReadUInt32();
		Seek(size, os::FILE_SEEK_CURRENT);
	}

	String *ReadString();
	String *ReadStringOrNull();
	char *ReadCString();

	inline VM *GetVM() const
	{
		return vm;
	}

	inline GC *GetGC() const
	{
		return vm->GetGC();
	}

private:
	String *ReadShortString(const int32_t length);
	String *ReadLongString(const int32_t length);

	void HandleError(os::FileStatus error);

	static const int MaxShortStringLength = 128;
};

} // namespace ovum
