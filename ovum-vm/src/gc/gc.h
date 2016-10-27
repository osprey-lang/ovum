#pragma once

#include "../vm.h"
#include "gcobject.h"
#include "stringtable.h"
#include "../threading/sync.h"

namespace ovum
{

// This is identical to String except that all the 'const' modifiers
// have been removed. There's a damn good reason String::length and
// String::firstChar are const. Do not use MutableString unless you
// know exactly what you're doing.
// There are exceptionally few circumstances that warrant the use
// of mutable strings.
// IF STRING CHANGES, MUTABLESTRING MUST BE UPDATED TO REFLECT THAT.
struct MutableString
{
	uint32_t length;
	uint32_t hashCode;
	StringFlags flags;
	ovchar_t firstChar;
};

class GC
{
public:
	// Creates a garbage collector instance.
	OVUM_NOINLINE static Box<GC> New(VM *owner);

	~GC();

	inline uint32_t GetCollectCount() const
	{
		return collectCount;
	}

	inline VM *GetVM() const
	{
		return vm;
	}

	int Alloc(Thread *const thread, Type *type, size_t size, GCObject **output);
	int Alloc(Thread *const thread, Type *type, size_t size, Value *output);

	int AllocArray(Thread *const thread, uint32_t length, uint32_t itemSize, void **output);

	int AllocValueArray(Thread *const thread, uint32_t length, Value **output);

	String *ConstructString(Thread *const thread, int32_t length, const ovchar_t value[]);

	String *ConvertString(Thread *const thread, const char *string);

	String *ConstructModuleString(Thread *const thread, int32_t length, const ovchar_t value[]);

	String *GetInternedString(Thread *const thread, String *value);

	bool HasInternedString(Thread *const thread, String *value);

	String *InternString(Thread *const thread, String *value);

	int Construct(Thread *const thread, Type *type, ovlocals_t argc, Value *output);

	int ConstructLL(Thread *const thread, Type *type, ovlocals_t argc, Value *args, Value *output);

	void AddMemoryPressure(Thread *const thread, size_t size);

	void RemoveMemoryPressure(Thread *const thread, size_t size);

	StaticRef *AddStaticReference(Thread *const thread, Value *value);

	void Collect(Thread *const thread, bool collectGen1);

private:
	static const size_t LARGE_OBJECT_SIZE = 87040;
	static const intptr_t GC_VALUE_ARRAY = (intptr_t)1;

	// The current bit pattern used for coloring an object white and black,
	// respectively. These start out as 1 and 3, respectively, and are swapped
	// after each GC cycle.
	// The value GCOFlags::GRAY is also used, and does not change.
	GCOFlags currentWhite;
	GCOFlags currentBlack;

	char *gen0Current;
	void *gen0Base;
	void *gen0End;
	os::HeapHandle mainHeap;
	os::HeapHandle largeObjectHeap;
	
	GCObject *collectList;
	GCObject *pinnedList;

	// The total size of generation 1, not including unmanaged data.
	size_t gen1Size;

	uint32_t collectCount;

	StringTable strings;
	Box<StaticRefBlock> staticRefs;

	// Critical section that must be entered any time a function modifies
	// or accesses GC data that could interfere with a GC cycle, such as
	// Alloc or AddStaticReference.
	CriticalSection allocSection;

	// The VM instance that owns the GC.
	VM *vm;

	GC(VM *owner);

	bool InitializeHeaps();

	void DestroyHeaps();

	GCObject *AllocRaw(size_t size);

	GCObject *AllocRawGen1(size_t size);

	void ReleaseRaw(GCObject *gco);

	// Acquires exclusive access to the allocation lock.
	// If this lock cannot be acquired immediately, the thread spins
	// for a bit, then sleeps, until the lock becomes available.
	// During this waiting, the GC also marks the thread as being in
	// an unmanaged region. This is to prevent deadlocks, in case the
	// thread that currently owns the lock causes a GC cycle to run:
	// without entering an unmanaged region, the GC cycle thread would
	// wait indefinitely for this thread to suspend itself, which in
	// turn is waiting for the GC cycle thread to release the allocation
	// lock, which won't happen until the cycle has ended.
	void BeginAlloc(Thread *const thread);

	// Releases the allocation lock, allowing any waiting threads to
	// jump in and start allocating memory.
	void EndAlloc();

	void RunCycle(Thread *const thread, bool collectGen1);

	void BeginCycle(Thread *const thread);

	void EndCycle(Thread *const thread);

	void Release(GCObject *gco);

	void MoveGen0Survivors(LiveObjectFinder &liveFinder);

	void MoveSurvivorToGen1(LiveObjectFinder &liveFinder, GCObject *gco);

	void UpdateGen0References(LiveObjectFinder &liveFinder);

	void CollectGarbage(LiveObjectFinder &liveFinder, bool collectGen1);

	void AddPinnedObject(GCObject *gco);

	static GCObject *FlattenPinnedTree(GCObject *root, GCObject **lastItem);

	friend class LiveObjectFinder;
	friend class MovedObjectUpdater;
	friend class ObjectGraphWalker;
	friend class RootSetWalker;
	friend class VM;
};

} // namespace ovum
