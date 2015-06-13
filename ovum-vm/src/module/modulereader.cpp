#include "modulereader.h"
#include "module.h"
#include "../gc/gc.h"
#include <memory>

namespace ovum
{

ModuleReader::ModuleReader(VM *owner)
	: fileName(256), stream(),
	buffer(nullptr), bufferPosition(0),
	bufferIndex(0), bufferDataSize(0),
	vm(owner)
{ }
ModuleReader::~ModuleReader()
{
	if (os::FileHandleIsValid(&stream))
		os::CloseFile(&stream);
	free(buffer);
}

void ModuleReader::Open(const pathchar_t *fileName)
{
	if (!buffer)
	{
		buffer = reinterpret_cast<uint8_t*>(malloc(BUFFER_SIZE));
		if (!buffer)
			throw ModuleLoadException(fileName, "Not enough memory for file buffer.");
	}
	this->fileName.Append(fileName);

	os::FileStatus r = os::OpenFile(
		fileName,
		os::FILE_OPEN,
		os::FILE_ACCESS_READ,
		os::FILE_SHARE_READ,
		&stream
	);
	if (r != os::FILE_OK)
		HandleError(r);
}
void ModuleReader::Open(const PathName &fileName)
{
	Open(fileName.GetDataPointer());
}

void ModuleReader::Read(void *dest, uint32_t count)
{
	char *destp = (char*)dest;

	// First, let's read as much of our buffer as we can.
	if (bufferDataSize != 0 && bufferIndex < bufferDataSize)
	{
		uint32_t bufCount = min(bufferDataSize - bufferIndex, count);
		memcpy(destp, buffer + bufferIndex, bufCount);
		bufferIndex += bufCount;
		count -= bufCount;
		destp += bufCount;
	}

	// If we reach this point, then either there was no buffered data,
	// or we've exhausted the existing buffer. If the remaining number
	// of bytes to read is greater than BUFFER_SIZE, don't bother with
	// our buffer; just read straight into destp.
	if (count > BUFFER_SIZE)
	{
		while (count > 0)
		{
			uint32_t bytesRead = ReadRaw(destp, count);
			count -= bytesRead;
			destp += bytesRead;
		}
		// Make sure to invalidate the buffer
		bufferDataSize = 0;
		bufferIndex = 0;
	}
	else
	{
		// Otherwise, repopulate the buffer and try to read the requested
		// number of bytes. Remember that FillBuffer() may not actually
		// fill the entire buffer, so we need to keep refilling it until
		// we either hit EOF or we've read all the bytes we need.
		// Under most circumstances, this loop will not run more than once,
		// but it can't hurt to be thorough.
		while (count > 0)
		{
			// Must be at the end of the buffer here
			OVUM_ASSERT(bufferDataSize == 0 || bufferIndex == bufferDataSize);

			FillBuffer();
			OVUM_ASSERT(bufferIndex == 0);

			uint32_t bufCount = min(bufferDataSize, count);
			memcpy(destp, buffer, bufCount);

			bufferIndex += bufCount;
			count -= bufCount;
			destp += bufCount;
		}
	}
}

uint32_t ModuleReader::ReadRaw(void *dest, uint32_t count)
{
	size_t bytesRead;
	os::FileStatus r = os::ReadFile(&stream, (size_t)count, dest, &bytesRead);
	if (r != os::FILE_OK)
		HandleError(r);
	if (bytesRead == 0)
		HandleError(os::FILE_EOF);
	return (uint32_t)bytesRead;
}

unsigned long ModuleReader::GetFilePosition()
{
	int64_t position;
	os::FileStatus r = os::SeekFile(&stream, 0, os::FILE_SEEK_CURRENT, &position);
	if (r != os::FILE_OK)
		HandleError(r);
	return (unsigned long)position;
}

void ModuleReader::FillBuffer()
{
	bufferPosition = GetFilePosition();
	bufferDataSize = ReadRaw(buffer, BUFFER_SIZE);
	bufferIndex = 0;
}

unsigned long ModuleReader::GetPosition()
{
	if (bufferDataSize == 0)
		// No data read, query the file pointer
		return GetFilePosition();
	return bufferPosition + bufferIndex;
}

void ModuleReader::Seek(long amount, os::SeekOrigin origin)
{
	if (origin == os::FILE_SEEK_CURRENT)
	{
		bufferIndex += amount;
		if (bufferIndex < bufferDataSize)
			// The new offset is within the buffer, all is well
			return;
		// Otherwise, fall through to reset buffer and seek
		// the file pointer.
	}
	else if (origin == os::FILE_SEEK_START)
	{
		bufferIndex = amount - bufferPosition;
		if (bufferIndex < bufferDataSize)
			// The new offset is within the buffer, all is well
			return;
		// Otherwise, fall through to reset buffer and seek
		// the file pointer.
	}

	// Seeking from end or resetting and seeking outside the existing buffer.

	// Reset buffer
	bufferIndex = 0;
	bufferDataSize = 0;

	os::FileStatus r = os::SeekFile(&stream, (int64_t)amount, origin, nullptr);
	if (r != os::FILE_OK)
		HandleError(r);
}

String *ModuleReader::ReadString()
{
	const int32_t length = ReadInt32();

	if (length <= MaxShortStringLength)
		return ReadShortString(length);
	else
		return ReadLongString(length);
}
String *ModuleReader::ReadStringOrNull()
{
	const int32_t length = ReadInt32();

	if (length == 0)
		return nullptr;

	if (length <= MaxShortStringLength)
		return ReadShortString(length);
	else
		return ReadLongString(length);
}

char *ModuleReader::ReadCString()
{
	const int32_t length = ReadInt32();

	if (length == 0)
		return nullptr;

	std::unique_ptr<char[]> output(new char[length]);
	Read(output.get(), length);

	return output.release();
}

String *ModuleReader::ReadShortString(const int32_t length)
{
	LitString<MaxShortStringLength> buf = { length, 0, StringFlags::STATIC };
	// Fill the buffer with contents from the file
	Read(const_cast<uchar*>(buf.chars), sizeof(uchar) * length);

	String *intern = GetGC()->GetInternedString(nullptr, buf.AsString());
	if (intern == nullptr)
	{
		// Not interned, have to allocate!
		intern = GetGC()->ConstructModuleString(nullptr, length, buf.chars);
		GetGC()->InternString(nullptr, intern);
	}

	return intern;
}
String *ModuleReader::ReadLongString(const int32_t length)
{
	std::unique_ptr<uchar[]> data(new uchar[length + 1]);

	// Note: the module file does NOT include a terminating \0!
	Read(data.get(), sizeof(uchar) * length);

	// If a string with this value is already interned, we get that string instead.
	// If we have that string, GC::InternString does nothing; if we don't, we have
	// a brand new string and interning it actually interns it.
	String *string = GetGC()->ConstructModuleString(nullptr, length, data.get());
	string = GetGC()->InternString(nullptr, string);

	return string;
}

void ModuleReader::HandleError(os::FileStatus error)
{
	const char *message;
	switch (error)
	{
	case os::FILE_NOT_FOUND:     message = "The file could not be found.";   break;
	case os::FILE_ACCESS_DENIED: message = "Access to the file was denied."; break;
	case os::FILE_EOF:           message = "Unexpected end of file.";        break;
	default:                     message = "Unspecified I/O error.";         break;
	}
	throw ModuleIOException(message);
}

} // namespace ovum