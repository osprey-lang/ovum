#pragma once

#ifndef VM__GC_INTERNAL_H
#define VM__GC_INTERNAL_H

#include <cassert>
#include <atomic>
#include "ov_vm.internal.h"
#include "string_table.internal.h"
#include "critical_section.internal.h"

enum class GCOFlags : uint32_t
{
	NONE          = 0x0000,
	// The mark occupies the lowest two bits.
	// Collectible objects are marked with currentCollectMark,
	// which changes each cycle.
	// To obtain the marks of the current cycle, use these macros:
	//   GCO_COLLECT(currentCollectMark)
	//   GCO_PROCESS(currentCollectMark)
	//   GCO_KEEP(currentCollectMark)
	MARK          = 0x0003, // Mask for extracting the mark.

	// The GCObject represents a string allocated before the
	// standard String type was loaded.
	EARLY_STRING  = 0x0004,

	// The GCObject cannot be moved by the GC. This flag is only
	// relevant for gen0 objects.
	PINNED        = 0x0008,

	// The GCObject is in generation 0. This flag cannot be used
	// together with GEN_1 or LARGE_OBJECT.
	GEN_0         = 0x0010,
	// The GCObject is in generation 1. This flag cannot be used
	// together with GEN_0 or LARGE_OBJECT.
	GEN_1         = 0x0020,
	// The GCObject is in the large object heap. These objects
	// are never moved. This flag cannot be used together with
	// GEN_0 or GEN_1.
	LARGE_OBJECT  = 0x0040,
	// Mask for extracting the age
	GENERATION    = 0x0070,

	// The GCObjects has references to gen0 objects. This flag is
	// only set during a GC cycle, and is cleared as soon as all
	// gen0 references have been updated.
	HAS_GEN0_REFS = 0x0080,

	// The GCObject has been moved to generation 1. The newAddress
	// field contains the new address.
	MOVED         = 0x0100,
};
ENUM_OPS(GCOFlags, uint32_t);

// These GCO marks are always in the range 1–3
#define GCO_COLLECT(ccm) ((::GCOFlags)((ccm) + 1))
#define GCO_PROCESS(ccm) ((::GCOFlags)(((ccm) + 1) % 3 + 1))
#define GCO_KEEP(ccm)    ((::GCOFlags)(((ccm) + 2) % 3 + 1))

class GCObject
{
public:
	GCOFlags flags; // Collection flag
	size_t size; // The size of the GCObject + fields.

	uint32_t pinCount;
	uint32_t hashCode;

	GCObject *prev; // Pointer to the previous GC object in the object's list (collect, process or keep).
	GCObject *next; // Pointer to the next GC object in the object's list.

	// A flag that is set while a thread is reading from or writing to
	// a field of this instance. No other threads can read from or write
	// to any field of the instance while this flag is set. This is to
	// prevent race conditions, as Value cannot be read or written atomically.
	std::atomic_flag fieldAccessFlag;

	union
	{
		// The managed type of the GCObject.
		Type *type;
		// If the GCObject has been moved from gen0 to gen1,
		// this contains the new location of the object.
		GCObject *newAddress;
	};

	// The first field of the Value immediately follows the type;
	// this is the base of the Value's instance pointer.

	inline void Mark(GCOFlags mark)
	{
		flags = flags & ~GCOFlags::MARK | mark;
	}

	inline bool IsPinned()    { return (flags & GCOFlags::PINNED) == GCOFlags::PINNED; }
	inline bool HasGen0Refs() { return (flags & GCOFlags::HAS_GEN0_REFS) == GCOFlags::HAS_GEN0_REFS; }

	uint8_t *InstanceBase();
	uint8_t *InstanceBase(Type *type);
	Value *FieldsBase();
	Value *FieldsBase(Type *type);

	// Inserts a GCObject into a linked list.
	// The parameter 'list' points to the first object in the list.
	//
	// For performance reasons, this method does not remove the GCO from
	// the previous list it's in. Call RemoveFromList() first, unless you
	// know the GCO isn't in any list.
	inline void InsertIntoList(GCObject **list)
	{
		// Before insertion:
		// nullptr  <--  *list  <->  (*list)->next
		// After insertion:
		// nullptr  <--  gco  <->  *list  <->  (*list)->next
		this->prev = nullptr;  // gco is the first value, so it has nothing prior to it.
		this->next = *list; // The next value is the current base of the list.
		if (*list)
			(*list)->prev = this;
		*list = this;       // And then we update the base of the list!
	}
	// Removes a GCObject from its associated linked list, which is passed
	// as a parameter (not stored with the GCObject).
	//
	// This should always be called before calling InsertIntoList, which
	// does not automatically call this method for performance reasons.
	//
	// NOTE: also for performance reasons, this code does NOT set gco's
	// next and prev fields to null. RemoveFromList will be called mostly
	// before calling InsertIntoList, which writes to those fields.
	// If you need these fields to be null, you must set them yourself.
	// Call ClearLinks() on the GCO to accomplish this.
	inline void RemoveFromList(GCObject **list)
	{
		GCObject *prev = this->prev;
		GCObject *next = this->next;
		// This code maintains two important facts:
		//   1. If gco->prev == nullptr (that is, gco is the first object in the list),
		//      then gco->next->prev will also be null.
		//   2. If gco->next == nullptr (that is, it's the last object in the list),
		//      then gco->prev->next will also be null.

		// If this is the only object in the list, then this == *list
		// and next == nullptr, so list will correctly be set to null.
		if (this == *list)
			*list = next;

		// Before removal:
		// prev  <->  gco  <->  next
		// After removal:
		// prev  <->  next
		if (prev) prev->next = next;
		if (next) next->prev = prev;
	}
	inline void ClearLinks()
	{
		prev = nullptr;
		next = nullptr;
	}

	static GCObject *FromInst(void *inst);
	static GCObject *FromValue(Value *value);
};

static const size_t GCO_SIZE = ALIGN_TO(sizeof(GCObject), 8);

inline uint8_t *GCObject::InstanceBase()
{
	return (uint8_t*)this + GCO_SIZE;
}
inline uint8_t *GCObject::InstanceBase(Type *type)
{
	return (uint8_t*)this + GCO_SIZE + type->fieldsOffset;
}
inline Value *GCObject::FieldsBase()
{
	return (Value*)((char*)this + GCO_SIZE);
}
inline Value *GCObject::FieldsBase(Type *type)
{
	return (Value*)((char*)this + GCO_SIZE + type->fieldsOffset);
}

inline GCObject *GCObject::FromInst(void *inst)
{
	return reinterpret_cast<GCObject*>((char*)inst - GCO_SIZE);
}
inline GCObject *GCObject::FromValue(Value *value)
{
	return FromInst(value->instance);
}

// This is identical to String except that all the 'const' modifiers
// have been removed. There's a damn good reason String::length and
// String::firstChar are const. Do not use MutableString unless you
// know exactly what you're doing.
// There are exceptionally few circumstances that warrant the use
// of mutable strings.
// IF STRING CHANGES, MUTABLESTRING MUST BE UPDATED TO REFLECT THAT.
typedef struct MutableString_S
{
	uint32_t length;
	uint32_t hashCode;
	StringFlags flags;
	uchar firstChar;
} MutableString;

class StaticRef
{
private:
	std::atomic_flag accessFlag;
	Value value;

public:
	// Note: no constructor. The type needs to be usable in an array.

	// Initializes the static reference to the specified value.
	// This should only be called ONCE per static reference.
	inline void Init(Value value)
	{
		accessFlag = std::atomic_flag();
		this->value = value;
	}

	// Atomically reads the value of the static reference.
	inline Value Read()
	{
		using namespace std;
		while (accessFlag.test_and_set(memory_order_acquire))
			;
		Value result = value;
		accessFlag.clear(memory_order_release);
		return result;
	}
	inline void Read(Value *target)
	{
		using namespace std;
		while (accessFlag.test_and_set(memory_order_acquire))
			;
		*target = value;
		accessFlag.clear(memory_order_release);
	}

	// Atomically updates the value of the static reference.
	inline void Write(Value value)
	{
		using namespace std;
		while (accessFlag.test_and_set(memory_order_acquire))
			;
		this->value = value;
		accessFlag.clear(memory_order_release);
	}
	inline void Write(Value *value)
	{
		using namespace std;
		while (accessFlag.test_and_set(memory_order_acquire))
			;
		this->value = *value;
		accessFlag.clear(memory_order_release);
	}

	inline Value *GetValuePointer()
	{
		return &value;
	}

	friend class GC;
};

class StaticRefBlock;
class StaticRefBlock
{
public:
	StaticRefBlock *next;
	unsigned int count;
	// Only used during collection. Set to true if the block
	// contains any references to gen0 objects.
	bool hasGen0Refs;

	static const size_t BLOCK_SIZE = 64;
	StaticRef values[BLOCK_SIZE];

	inline StaticRefBlock() : next(nullptr), count(0), hasGen0Refs(false) { }
	inline StaticRefBlock(StaticRefBlock *next) : next(next), count(0), hasGen0Refs(0) { }
};

class GC
{
private:
	static const size_t GEN0_SIZE = 1536 * 1024;
	static const size_t LARGE_OBJECT_SIZE = 87040;
	// If there is more than this amount of dead memory in gen1,
	// that generation is always collected.
	static const size_t GEN1_DEAD_OBJECTS_THRESHOLD = 768 * 1024;

	typedef struct
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
	} Survivors;

	// The current bit pattern used for marking an object as "collect".
	// This changes every GC cycle.
	int currentCollectMark;

	HANDLE mainHeap;
	HANDLE largeObjectHeap;
	void *gen0Base;
	void *gen0End;
	char *gen0Current;
	
	GCObject *collectBase;
	GCObject *processBase;
	GCObject *keepBase;
	GCObject *pinnedBase;
	// This field is only assigned during a GC cycle, and points to
	// a location on the call stack. It should be set to null in all
	// other situations.
	Survivors *survivors;

	// The total size of generation 1, not including unmanaged data.
	size_t gen1Size;

	uint32_t collectCount;

	StringTable strings;
	StaticRefBlock *staticRefs;

	CriticalSection allocSection;

	GCObject *AllocRaw(size_t size);
	GCObject *AllocRawGen1(size_t size);
	void ReleaseRaw(GCObject *gco);

	void InitializeHeaps();
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
	// Initializes the garbage collector.
	NOINLINE static void Init();
	// Unloads the garbage collector.
	NOINLINE static void Unload();

	GC();
	~GC();

	void Alloc(Thread *const thread, Type *type, size_t size, GCObject **output);
	inline void Alloc(Thread *const thread, Type *type, size_t size, Value *output)
	{
		GCObject *gco;
		Alloc(thread, type, size, &gco);
		output->type = type;
		output->instance = gco->InstanceBase();
	}

	String *ConstructString(Thread *const thread, const int32_t length, const uchar value[]);
	String *ConvertString(Thread *const thread, const char *string);

	String *ConstructModuleString(Thread *const thread, const int32_t length, const uchar value[]);

	inline String *GetInternedString(String *value)
	{
		return strings.GetInterned(value);
	}
	inline bool HasInternedString(String *value)
	{
		return strings.HasInterned(value);
	}
	inline String *InternString(String *value)
	{
		return strings.Intern(value);
	}

	void Construct(Thread *const thread, Type *type, const uint16_t argc, Value *output);
	void ConstructLL(Thread *const thread, Type *type, const uint16_t argc, Value *args, Value *output);

	void AddMemoryPressure(Thread *const thread, const size_t size);
	void RemoveMemoryPressure(Thread *const thread, const size_t size);

	StaticRef *AddStaticReference(Value value);

	void Collect(Thread *const thread, bool collectGen1);

private:
	void BeginCycle(Thread *const thread);
	void EndCycle(Thread *const thread);

	void Release(GCObject *gco);

	static inline unsigned int LinkedListLength(GCObject *first)
	{
		unsigned int count = 0;

		while (first)
		{
			count++;
			first = first->next;
		}

		return count;
	}

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
	inline bool ShouldProcess(Value *val, bool *hasGen0Refs)
	{
		if (val->type == nullptr || val->type->IsPrimitive())
			return false;

		if (val->type == VM::vm->types.String &&
			(val->common.string->flags & StringFlags::STATIC) == StringFlags::STATIC)
			return false;

		// If gco is a non-pinned gen0 object, set *hasGen0Refs to true.
		GCOFlags flags = GCObject::FromValue(val)->flags;
		if ((flags & GCOFlags::GEN_0) == GCOFlags::GEN_0 &&
			(flags & GCOFlags::PINNED) == GCOFlags::NONE)
			*hasGen0Refs = true;

		return (flags & GCOFlags::MARK) == GCO_COLLECT(currentCollectMark);
	}
	inline void TryMarkForProcessing(Value *value, bool *hasGen0Refs)
	{
		if (ShouldProcess(value, hasGen0Refs))
			MarkForProcessing(GCObject::FromValue(value));
	}
	inline void TryMarkStringForProcessing(String *str, bool *hasGen0Refs)
	{
		if ((str->flags & StringFlags::STATIC) == StringFlags::NONE)
		{
			GCObject *gco = GCObject::FromInst(str);
			if ((gco->flags & GCOFlags::GEN_0) == GCOFlags::GEN_0)
				*hasGen0Refs = true;
			if ((gco->flags & GCOFlags::MARK) == GCO_COLLECT(currentCollectMark))
				MarkForProcessing(gco);
		}
	}

	void ProcessObjectAndFields(GCObject *gco);
	void ProcessCustomFields(Type *type, void *instBase, bool *hasGen0Refs);
	void ProcessHash(HashInst *hash, bool *hasGen0Refs);
	inline void ProcessFields(unsigned int fieldCount, Value fields[], bool *hasGen0Refs)
	{
		for (unsigned int i = 0; i < fieldCount; i++)
			// If the object is marked GCO_KEEP, we're done processing it.
			// If it's marked GCO_PROCESS, it'll be processed eventually.
			// Otherwise, mark it for processing!
			TryMarkForProcessing(fields + i, hasGen0Refs);
	}

	void MoveGen0Survivors();
	void AddPinnedObject(GCObject *gco);
	static GCObject *FlattenPinnedTree(GCObject *root, GCObject **lastItem);

	void UpdateGen0References();
	void UpdateRootSet();
	static void UpdateObjectFields(GCObject *gco);
	static void UpdateCustomFields(Type *type, void *instBase);
	static void UpdateHash(HashInst *hash);

	static inline bool ShouldUpdateRef(Value *val)
	{
		if (val->type == nullptr || val->type->IsPrimitive())
			return false;

		if (val->type == VM::vm->types.String &&
			(val->common.string->flags & StringFlags::STATIC) == StringFlags::STATIC)
			return false;

		return (GCObject::FromValue(val)->flags & GCOFlags::MOVED) == GCOFlags::MOVED;
	}
	static inline void TryUpdateRef(Value *value)
	{
		if (ShouldUpdateRef(value))
			value->instance = GCObject::FromValue(value)->newAddress->InstanceBase();
	}
	static inline void TryUpdateStringRef(String **str)
	{
		if (((*str)->flags & StringFlags::STATIC) == StringFlags::NONE)
		{
			GCObject *gco = GCObject::FromInst(*str);
			if ((gco->flags & GCOFlags::MOVED) == GCOFlags::MOVED)
				*str = (String*)gco->newAddress->InstanceBase();
		}
	}
	static inline void UpdateFields(unsigned int fieldCount, Value fields[])
	{
		for (unsigned int i = 0; i < fieldCount; i++)
			TryUpdateRef(fields + i);
	}

public:
	static GC *gc;

	friend class VM;
};

static const size_t GC_SIZE = sizeof(GC);

#endif // VM__GC_INTERNAL_H