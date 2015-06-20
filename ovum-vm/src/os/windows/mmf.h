#ifndef VM__OS_MMAP_H
#define VM__OS_MMAP_H

#include "def.h"
#include "../../vm.h"

// Note: The functions exported by this header file depend on pathchar_t,
// which is defined in Ovum's public headers. If you require a special
// definition of pathchar_t, modify ov_pathchar.h.

namespace ovum
{

namespace os
{

	typedef struct {
		FileHandle file;
		HANDLE mapping;
	} MemoryMappedFile;

	// Specifies how the memory-mapped file may be used, once mapped
	// into memory. Due to inconsistencies between OSes, these values
	// should not be combined as flags.
	// Some systems may allow a file to be mapped into an executable
	// view even if it was opened without one of the _EXEC vaules in
	// this enum. For maximum compatibility, always use the appropriate
	// appropriate value both when opening the file and when creating
	// a mapped view of it.
	enum MmfAccess
	{
		// The memory-mapped file may only be used for reading. Pages mapped
		// from the file will not be writable or executable.
		MMF_OPEN_READ = PAGE_READONLY,
		// The memory-mapped file may used for writing. This also grants the
		// permission to read pages mapped from the file, but not execute them.
		MMF_OPEN_WRITE = PAGE_READWRITE,
		// The memory-mapped file may used for reading or executing. Pages
		// mapped from the file will not be writable.
		MMF_OPEN_READ_EXEC = PAGE_EXECUTE_READ,
		// The memory-mapped file may be used for reading, writing or executing.
		// This effectively grants all permissions.
		MMF_OPEN_WRITE_EXEC = PAGE_EXECUTE_READWRITE,
		// The memory-mapped file may be used for reading and writing, and
		// permits views to be mapped with MMF_VIEW_PRIVATE. See details under
		// that value.
		MMF_OPEN_PRIVATE = PAGE_WRITECOPY,
		// The memory-mapped file may be used for reading, writing or executing,
		// and iews may be mapped with MMF_VIEW_PRIVATE or MMF_VIEW_PRIVATE_EXEC.
		// See details under those values.
		MMF_OPEN_PRIVATE_EXEC = PAGE_EXECUTE_WRITECOPY,
	};

	// Specifies how the memory of a mapped view of a file may be used.
	// Due to inconsistencies between OSes, these values should not be
	// combined as flags.
	enum MmfViewAccess
	{
		// The view can be read from, but not written or executed.
		MMF_VIEW_READ = FILE_MAP_READ,
		// The view can be read from or executed, but not written.
		// The file must have been opened with MMF_OPEN_READ_EXEC,
		// MMF_OPEN_WRITE_EXEC or MMF_OPEN_PRIVATE_EXEC.
		MMF_VIEW_READ_EXEC = FILE_MAP_READ | FILE_MAP_EXECUTE,
		// The view can be read from or written to, but not executed.
		// The file must have been opened with MMF_OPEN_WRITE, MMF_OPEN_WRITE_EXEC,
		// MMF_OPEN_PRIVATE or MMF_OPEN_PRIVATE_EXEC.
		MMF_VIEW_WRITE = FILE_MAP_WRITE,
		// The view can be read from, written to or executed.
		// The file must have been opened with MMF_OPEN_WRITE_EXEC
		// or MMF_OPEN_PRIVATE_EXEC.
		MMF_VIEW_WRITE_EXEC = FILE_MAP_WRITE | FILE_MAP_EXECUTE,
		// The view can be read from or written to, but not executed. In
		// addition, changes are not written to the underlying file. Instead,
		// a private copy is allocated when a page is modified. The file must
		// have been opened with MMF_OPEN_PRIVATE or MMF_OPEN_PRIVATE_EXEC.
		MMF_VIEW_PRIVATE = FILE_MAP_COPY,
		// The view can be read from, written to or executed. In addition,
		// changes are not written to the underlying file. Instead, a private
		// copy is allocated when a page is modified. The file must have been
		// opened with MMF_OPEN_PRIVATE or MMF_OPEN_PRIVATE_EXEC.
		MMF_VIEW_PRIVATE_EXEC = FILE_MAP_COPY | FILE_MAP_EXECUTE,
	};

	// Gets the required file offset alignment of a view from
	// a memory-mapped file. When mapping a view of a file, the
	// file offset must be aligned to a multiple of this value.
	// Note: this value may or may not differ from the page size
	// or allocation granularity. Always use this method to get
	// the file offset alignment of memory-mapped file views.
	inline size_t GetMmfViewAlignment()
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		return (size_t)sysInfo.dwAllocationGranularity;
	}

	// Opens a file with the specified name for memory-mapping.
	//   name:
	//     The name of the file to open.
	//   mode:
	//     Specifies how the file is opened. See FileMode's members
	//     for a description of each mode.
	//   access:
	//     Specifies how views of the file are allowed to be mapped.
	//   share:
	//     Specifies whether other processes can open the file at
	//     the same time, and if so, what they can do with it.
	//   output:
	//     Receives a handle to the mapped file, if the call succeeds.
	// Returns:
	//   A status code which indicates whether an error occurred. If
	//   the file was opened successfully, FILE_OK is returned.
	FileStatus OpenMemoryMappedFile(const pathchar_t *name, FileMode mode, MmfAccess access, FileShare share, MemoryMappedFile *output);

	// Closes a memory-mapped file. Views mapped from the file
	// may or may not become invalidated, depending on the OS.
	// Do not use such views after closing the file.
	//   file:
	//     The file to close.
	// Returns:
	//   A status code which indicates whether an error occurred. If
	//   the file was opened successfully, FILE_OK is returned.
	inline FileStatus CloseMemoryMappedFile(MemoryMappedFile *file)
	{
		// Always attempt to close both the mapping and the file.
		// If either fails, return an error code.
		BOOL mappingResult = ::CloseHandle(file->mapping);
		FileStatus fileResult = CloseFile(&file->file);
		if (!mappingResult)
			return FILE_IO_ERROR;
		if (fileResult != FILE_OK)
			return fileResult;
		return FILE_OK;
	}

	// Maps a portion of the file into memory. The memory can then
	// be accessed as specified.
	//   file:
	//     The file to map a view from.
	//   access:
	//     Specifies how the memory may be used.
	//   offset:
	//     The file offset at which the mapped region begins. This must
	//     be a multiple of the value returned by GetMmfViewAlignment().
	//   size:
	//     The number of bytes to map into memory. If this parameter is 0,
	//     the view will extend to the end of the file.
	// Returns:
	//   The base address of the mapped view, or null if an error occurred.
	inline void *MapView(MemoryMappedFile *file, MmfViewAccess access, int64_t offset, size_t size)
	{
		return ::MapViewOfFile(file->mapping, (DWORD)access, offset >> 32, (DWORD)offset, size);
	}

	// Unmaps a view of the file. The virtual address range associated
	// with the mapping will no longer be usable.
	//   viewBase:
	//     The base address of the mapped view.
	inline void UnmapView(void *viewBase)
	{
		::UnmapViewOfFile(viewBase);
	}

} // namespace os

} // namespace ovum

#endif // VM__OS_MMAP_H