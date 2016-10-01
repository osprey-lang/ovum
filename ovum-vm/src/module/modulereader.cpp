#include "modulereader.h"
#include "module.h"
#include "../gc/gc.h"

namespace ovum
{

ModuleReader::ModuleReader(VM *owner) :
	fileName(256),
	vm(owner)
{ }
ModuleReader::~ModuleReader()
{
}

void ModuleReader::Open(const pathchar_t *fileName)
{
	this->fileName.Clear();
	this->fileName.Append(fileName);

	os::FileStatus r = os::OpenMemoryMappedFile(
		fileName,
		os::FILE_OPEN,
		os::MMF_OPEN_READ,
		os::FILE_SHARE_READ,
		&this->file
	);

	if (r != os::FILE_OK)
		HandleFileOpenError(r);
}
void ModuleReader::Open(const PathName &fileName)
{
	Open(fileName.GetDataPointer());
}

String *ModuleReader::ReadShortString(uint32_t address, int32_t length)
{
	LitString<MaxShortStringLength> buf = { length, 0, StringFlags::STATIC };

	// TODO: Fill the buffer with contents from the file

	String *intern = GetGC()->GetInternedString(nullptr, buf.AsString());
	if (intern == nullptr)
	{
		// Not interned, have to allocate!
		intern = GetGC()->ConstructModuleString(nullptr, length, buf.chars);
		GetGC()->InternString(nullptr, intern);
	}

	return intern;
}
String *ModuleReader::ReadLongString(uint32_t address, int32_t length)
{
	// Note: the module file does NOT include a terminating \0!
	std::unique_ptr<ovchar_t[]> data(new ovchar_t[length + 1]);

	// TODO: read data

	// If a string with this value is already interned, we get that string instead.
	// If we have that string, GC::InternString does nothing; if we don't, we have
	// a brand new string and interning it actually interns it.
	String *string = GetGC()->ConstructModuleString(nullptr, length, data.get());
	string = GetGC()->InternString(nullptr, string);

	return string;
}

void ModuleReader::HandleFileOpenError(os::FileStatus error)
{
	const char *message;
	switch (error)
	{
	case os::FILE_NOT_FOUND:     message = "The file could not be found.";   break;
	case os::FILE_ACCESS_DENIED: message = "Access to the file was denied."; break;
	default:                     message = "Unspecified I/O error.";         break;
	}
	throw ModuleIOException(message);
}

} // namespace ovum
