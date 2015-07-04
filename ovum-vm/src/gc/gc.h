#ifndef VM__GC_INTERNAL_H
#define VM__GC_INTERNAL_H

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
private:
	static const size_t GEN0_SIZE = 1536 * 1024;
	static const size_t LARGE_OBJECT_SIZE = 87040;
	// If there is more than this amount of dead memory in gen1,
	// that generation is always collected.
	static const size_t GEN1_DEAD_OBJECTS_THRESHOLD = 768 * 1024;
	static const intptr_t GC_VALUE_ARRAY = (intptr_t)1;

	struct TempLists
	{
		GCObject *process;
		GCObject *keep;
		struct
		{
			// All survivors from generation 0.
			GCObject *gen0;
			// All survivors with references to gen0 objects.
			// Initially only contains survivors from gen1 and
			// the large object heap, but is later updated to
			// include gen0 survivors with gen0 refs.
			GCObject *withGen0Refs;
			// Total size of gen1 survivors. This does NOT
			// include objects from the large object heap.
			size_t gen1SurvivorSize;
		} survivors;
	};

	// The current bit pattern used for marking an object as "collect",
	// and "keep", respectively. These start out as 1 and 3, respectively,
	// and are swapped after each GC cycle.
	// The value GCOFlags::PROCESS is also used, and does not change.
	GCOFlags currentCollectMark;
	GCOFlags currentKeepMark;

	char *gen0Current;
	void *gen0Base;
	void *gen0End;
	os::HeapHandle mainHeap;
	os::HeapHandle largeObjectHeap;
	
	GCObject *collectList;
	GCObject *pinnedList;
	// This field is only assigned during a GC cycle, and points to
	// a location on the call stack. It should be set to null in all
	// other situations.
	TempLists *gcoLists;

	// The total size of generation 1, not including unmanaged data.
	size_t gen1Size;

	uint32_t collectCount;

	StringTable strings;
	StaticRefBlock *staticRefs;

	// Critical section that must be entered any time a function modifies
	// or accesses GC data that could interfere with a GC cycle, such as
	// Alloc or AddStaticReference.
	CriticalSection allocSection;

	// The VM instance that owns the GC.
	VM *vm;

	GCObject *AllocRaw(size_t size);
	GCObject *AllocRawGen1(size_t size);
	void ReleaseRaw(GCObject *gco);

	bool InitializeHeaps();
	void DestroyHeaps();

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

public:
	// Creates a garbage collector instance.
	OVUM_NOINLINE static int Create(VM *owner, GC *&gc);

	GC(VM *owner);
	~GC();

	inline uint32_t GetCollectCount() const
	{
		return collectCount;
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

	StaticRef *AddStaticReference(Thread *const thread, Value value);

	void Collect(Thread *const thread, bool collectGen1);

	inline VM *GetVM() const
	{
		return vm;
	}

private:
	void RunCycle(Thread *const thread, bool collectGen1);
	void BeginCycle(Thread *const thread);
	void EndCycle(Thread *const thread);

	void Release(GCObject *gco);

	struct FieldProcessState
	{
		GC *gc;
		bool *hasGen0Refs;
	};

	void MarkForProcessing(GCObject *gco);
	void AddSurvivor(GCObject *gco);

	void MarkRootSet();

	// Determines whether a particular Value should be processed.
	// A Value should be processed if:
	//   1. Its type is not null.
	//   2. Its type is not PRIMITIVE.
	//   3. It is not a string with the flag STATIC (no associated GCObject).
	//   4. Its GCObject* is marked GCO_COLLECT.
	// NOTE: This function is only called for /reachable/ Values.
	bool ShouldProcess(Value *val, bool *hasGen0Refs);

	void TryMarkForProcessing(Value *value, bool *hasGen0Refs);

	void TryMarkStringForProcessing(String *str, bool *hasGen0Refs);

	void ProcessObjectAndFields(GCObject *gco);

	void ProcessCustomFields(Type *type, void *instBase, bool *hasGen0Refs);

	void ProcessFields(unsigned int fieldCount, Value fields[], bool *hasGen0Refs);

	void ProcessLocalValues(unsigned int count, Value values[]);

	static int ProcessFieldsCallback(void *state, unsigned int count, Value *values);

	void MoveGen0Survivors();

	void AddPinnedObject(GCObject *gco);

	static GCObject *FlattenPinnedTree(GCObject *root, GCObject **lastItem);

	void UpdateGen0References();

	void UpdateRootSet();

	void UpdateObjectFields(GCObject *gco);

	void UpdateCustomFields(Type *type, void *instBase);

	static int UpdateFieldsCallback(void *state, unsigned int count, Value *values);

	bool ShouldUpdateRef(Value *val);

	void TryUpdateRef(Value *value);

	static void TryUpdateStringRef(String **str);

	void UpdateFields(unsigned int fieldCount, Value fields[]);

	void UpdateLocals(unsigned int count, Value values[]);

	friend class VM;
};

} // namespace ovum

#endif // VM__GC_INTERNAL_H
