#include <memory>
#include "modulereader.internal.h"
#include "ov_module.internal.h"

ModuleReader::ModuleReader()
	: fileName(256), stream(nullptr),
	buffer(nullptr), bufferPosition(0),
	bufferIndex(0), bufferDataSize(0)
{ }
ModuleReader::~ModuleReader()
{
	if (stream != nullptr && stream != INVALID_HANDLE_VALUE)
		CloseHandle(stream);
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

	stream = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (stream == INVALID_HANDLE_VALUE)
		HandleError(GetLastError());
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
			assert(bufferDataSize == 0 || bufferIndex == bufferDataSize);

			FillBuffer();
			assert(bufferIndex == 0);

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
	DWORD bytesRead;
	BOOL result = ReadFile(stream, dest, count, &bytesRead, nullptr);
	if (!result)
		HandleError(GetLastError());
	if (bytesRead == 0)
		HandleError(ERROR_HANDLE_EOF);
	return bytesRead;
}

unsigned long ModuleReader::GetFilePosition()
{
	unsigned long result = SetFilePointer(stream, 0, nullptr, FILE_CURRENT);
	if (result == INVALID_SET_FILE_POINTER)
		HandleError(GetLastError());
	return result;
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

void ModuleReader::Seek(long amount, SeekOrigin origin)
{
	if (origin == SeekOrigin::CURRENT)
	{
		bufferIndex += amount;
		if (bufferIndex < bufferDataSize)
			// The new offset is within the buffer, all is well
			return;
		// Otherwise, fall through to reset buffer and seek
		// the file pointer.
	}
	else if (origin == SeekOrigin::BEGIN)
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

	// The SeekOrigin values are the same as FILE_BEGIN, FILE_CURRENT and FILE_END,
	// so we can just cast origin to DWORD.
	unsigned long newPos = SetFilePointer(stream, amount, nullptr, (DWORD)origin);
	if (newPos == INVALID_SET_FILE_POINTER)
		HandleError(GetLastError());
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
	using namespace std;

	const int32_t length = ReadInt32();

	if (length == 0)
		return nullptr;

	unique_ptr<char[]> output(new char[length]);
	Read(output.get(), length);

	return output.release();
}

String *ModuleReader::ReadShortString(const int32_t length)
{
	LitString<MaxShortStringLength> buf = { length, 0, StringFlags::STATIC };
	// Fill the buffer with contents from the file
	Read(const_cast<uchar*>(buf.chars), sizeof(uchar) * length);

	String *intern = GC::gc->GetInternedString(_S(buf));
	if (intern == nullptr)
	{
		// Not interned, have to allocate!
		intern = GC::gc->ConstructModuleString(nullptr, length, buf.chars);
		GC::gc->InternString(intern);
	}

	return intern;
}
String *ModuleReader::ReadLongString(const int32_t length)
{
	using namespace std;

	unique_ptr<uchar[]> data(new uchar[length + 1]);

	// Note: the module file does NOT include a terminating \0!
	Read(data.get(), sizeof(uchar) * length);

	// If a string with this value is already interned, we get that string instead.
	// If we have that string, GC::InternString does nothing; if we don't, we have
	// a brand new string and interning it actually interns it.
	String *string = GC::gc->ConstructModuleString(nullptr, length, data.get());
	string = GC::gc->InternString(string);

	return string;
}

void ModuleReader::HandleError(DWORD error)
{
	const char *message;
	switch (error)
	{
	case ERROR_FILE_NOT_FOUND: message = "The file could not be found.";   break;
	case ERROR_ACCESS_DENIED:  message = "Access to the file was denied."; break;
	case ERROR_HANDLE_EOF:     message = "Unexpected end of file.";        break;
	default:                   message = "Unspecified I/O error.";         break;
	}
	throw ModuleIOException(message);
}