#pragma once

#include "../vm.h"
#include "../ee/vm.h"
#include "../util/pathname.h"

namespace ovum
{

class ModuleReader
{
private:
	os::MemoryMappedFile file;

	PathName fileName;
	// The VM instance that the reader reads module data for.
	VM *vm;

public:
	ModuleReader(VM *owner);
	~ModuleReader();

	void Open(const pathchar_t *fileName);
	void Open(const PathName &fileName);

	inline const PathName &GetFileName() const
	{
		return fileName;
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

	void HandleFileOpenError(os::FileStatus err);

	static const int MaxShortStringLength = 128;
};

} // namespace ovum
