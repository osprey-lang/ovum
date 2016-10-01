#pragma once

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

	inline void *GetData() const
	{
		return data;
	}

private:
	// Memoy-mapped file contents
	void *data;

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
