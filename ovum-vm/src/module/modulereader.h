#pragma once

#include "modulefile.h"
#include "../vm.h"
#include "../ee/vm.h"
#include "../util/pathname.h"

namespace ovum
{

class ModuleFile
{
public:
	ModuleFile();
	~ModuleFile();

	void Open(const pathchar_t *fileName);
	void Open(const PathName &fileName);

	inline const PathName &GetFileName() const
	{
		return fileName;
	}

	inline const void *GetData() const
	{
		return data;
	}

	template<class T>
	inline const T *Read(uint32_t address) const
	{
		return reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + address
		);
	}

	template<class T>
	inline void Read(uint32_t address, T *output) const
	{
		*output = *reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + address
		);
	}

	template<class T>
	inline T *Deref(module_file::Rva<T> rva) const
	{
		return reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + rva.address
		);
	}

	template<class T>
	inline void Deref(module_file::Rva<T> rva, T *output) const
	{
		*output = *reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + rva.address
		);
	}

private:
	// Memory-mapped file contents
	const void *data;

	os::MemoryMappedFile file;
	PathName fileName;

	void HandleFileOpenError(os::FileStatus err);
};

class ModuleReader
{
private:
	ModuleFile file;

	// The VM instance that the reader reads module data for.
	VM *vm;

public:
	ModuleReader(VM *owner);
	~ModuleReader();

	void Open(const pathchar_t *fileName);
	void Open(const PathName &fileName);

	inline const ModuleFile &GetFile() const
	{
		return file;
	}

	inline const void *GetData() const
	{
		return file.GetData();
	}

	inline const PathName &GetFileName() const
	{
		return file.GetFileName();
	}

	inline VM *GetVM() const
	{
		return vm;
	}

	inline GC *GetGC() const
	{
		return vm->GetGC();
	}

private:
	String *ReadShortString(uint32_t address, int32_t length);
	String *ReadLongString(uint32_t address, int32_t length);

	static const int MaxShortStringLength = 128;
};

} // namespace ovum
