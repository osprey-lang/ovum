#pragma once

#ifndef VM__OS_MEM_H
#define VM__OS_MEM_H

#include "def.h"

namespace ovum
{

namespace os
{
	
	// Memory protection constants. Due to inconsistencies between OSes,
	// these should not be combined as flags, as that may not have the
	// desired result.
	enum MemoryProtection
	{
		// The memory cannot be accessed. Attempting to read from, write to
		// or execute code within the region will cause a segmentation violation.
		VPROT_NO_ACCESS = ...,
		// The memory should be readable. Attempting to write to the region
		// or execute code in it will cause a segmentation violation.
		VPROT_READ = ...,
		// The memory should be readable and writable. Attempting to execute
		// code in the region will cause a segmentation violation. 
		VPROT_READ_WRITE = ...,
		// The memory should be readable and executable. Attempting to write
		// to the region will cause a segmentation violation.
		VPROT_READ_EXEC = ...,
		// The memory should be readable, writable and executable.
		VPROT_READ_WRITE_EXEC = ...,
	};

	typedef ... HeapHandle;

	// Gets the page size on the current OS.
	size_t GetPageSize();

	// Allocates the specified number of bytes in the virtual address space.
	// Pages are not physically allocated until they are used.
	//   addr:
	//     The preferred address of the block of memory. The return value may
	//     or may not be at this address; callers should see this as nothing
	//     more than a hint. This parameter may be null to indicate that the
	//     caller has no preferred address.
	//   size:
	//     The number of bytes to allocate.
	//   protection:
	//     The protection to apply to the memory region.
	// Returns:
	//   A pointer to the start of the allocated region, or null if the memory
	//   could not be allocated.
	void *VirtualAlloc(void *const addr, size_t size, MemoryProtection protection);

	// Modifies the memory protection of a range of the virtual address space.
	//   addr:
	//     The starting address of the memory range.
	//   size:
	//     The number of bytes to change memory protection for. The actual minimum
	//     number of bytes that can be affected by this function is dependent on
	//     the OS, and will generally be rounded up to the page size.
	//   protection:
	//     The protection to apply to the memory region.
	// Returns:
	//   True if the memory protection was successfully changed; otherwise, false.
	bool VirtualProtect(void *const addr, size_t size, MemoryProtection protection);

	// Locks a region of virtual memory, preventing it from being moved to the
	// swap file.
	//   addr:
	//     The starting address of the memory range.
	//   size:
	//     The total number of bytes to lock.
	// Returns:
	//   True if the memory was successfully locked; otherwise, false.
	bool VirtualLock(void *const addr, size_t size);

	// Unlocks a region of virtual memory, allowing it to be swapped out.
	//   addr:
	//     The starting address of the memory range.
	//   size:
	//     The total number of bytes to unlock.
	// Returns:
	//   True if the memory was successfully unlocked; otherwise, false.
	bool VirtualUnlock(void *const addr, size_t size);

	// Frees memory previously allocated using VirtualAlloc. The memory becomes
	// available for others to use.
	//   addr:
	//     The base address of the memory to free. This must correspond to the
	//     return value of a call to VirtualAlloc. This parameter may be null,
	//     in which case nothing happens.
	// Returns:
	//   True if the operation succeeded; otherwise, false.
	bool VirtualFree(void *const addr);

	// Private heap functionality is not required to be supported. If it isn't,
	// the definitions of these functions should use an appropriate fallback,
	// such as malloc()/free(), and should define HeapHandle to be a pointer
	// to an empty struct.

	// Creates a new private heap with the specified initial size.
	//   heap:
	//     A pointer to the heap handle to initialize.
	//   initialSize:
	//     The initial size of the heap, in bytes. This value will
	//     almost certainly be rounded up by the OS.
	// Returns:
	//   True if successful; otherwise, false.
	bool HeapCreate(HeapHandle *heap, size_t initialSize);

	// Destroys a private heap. All the memory allocated in the heap
	// is deallocated, and all the pages are decommitted.
	//   heap:
	//     The heap to destroy.
	// Returns:
	//   True if successful; otherwise, false.
	bool HeapDestroy(HeapHandle *heap);

	// Allocates the specified amount of memory in a private heap.
	//   heap:
	//     The heap in which to allocate memory.
	//   size:
	//     The total number of bytes to allocate.
	//   zero:
	//     If true, the allocated region will have its contents
	//     zeroed before returning.
	// Returns:
	//   A pointer to the allocated region, or null if the memory
	//   could not be allocated.
	void *HeapAlloc(HeapHandle *heap, size_t size, bool zero);

	// Frees a region of memory from a private heap.
	//   heap:
	//     The heap in which the memory was allocated.
	//   mem:
	//     A pointer to the memory that is to be freed. This value
	//     should not be null.
	void HeapFree(HeapHandle *heap, void *mem);

} // namespace os

} // namespace ovum

#endif // VM__OS_MEM_H