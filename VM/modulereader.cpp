#include <memory>
#include "modulereader.internal.h"
#include "ov_module.internal.h"

ModuleReader::ModuleReader() : fileName(), stream(nullptr) { }
ModuleReader::~ModuleReader()
{
	if (stream != nullptr && stream != INVALID_HANDLE_VALUE)
		CloseHandle(stream);
}

void ModuleReader::Open(const wchar_t *fileName)
{
	this->fileName.append(fileName);

	stream = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (stream == INVALID_HANDLE_VALUE)
		HandleError(GetLastError());
}
void ModuleReader::Open(const std::wstring &fileName)
{
	Open(fileName.c_str());
}

void ModuleReader::Read(void *dest, uint32_t count)
{
	DWORD bytesRead;
	BOOL result = ReadFile(stream, dest, count, &bytesRead, nullptr);
	if (!result)
		HandleError(GetLastError());
	if (bytesRead < count)
		HandleError(ERROR_HANDLE_EOF);
}

long ModuleReader::GetPosition()
{
	long result = SetFilePointer(stream, 0, nullptr, FILE_CURRENT);
	if (result == INVALID_SET_FILE_POINTER)
		HandleError(GetLastError());
	return result;
}

void ModuleReader::Seek(long amount, SeekOrigin origin)
{
	// The SeekOrigin values are the same as FILE_BEGIN, FILE_CURRENT and FILE_END.
	if (SetFilePointer(stream, amount, nullptr, (DWORD)origin) == INVALID_SET_FILE_POINTER)
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