#pragma once

#ifndef VM__GC_INTERNAL_H
#define VM__GC_INTERNAL_H

#include <cassert>
#include <atomic>
#include "ov_vm.internal.h"
#include "string_table.internal.h"

enum class GCOFlags : uint32_t
{
	NONE  = 0x00,
	// The mark occupies the lowest two bits.
	// Collectible objects are marked with currentCollectMark,
	// which changes each cycle.
	// To obtain the marks of the current cycle, use these macros:
	//   GCO_COLLECT(currentCollectMark)
	//   GCO_PROCESS(currentCollectMark)
	//   GCO_KEEP(currentCollectMark)
	MARK  = 0x03, // Mask for extracting the mark.
	// The GCObject represents a string allocated before the
	// standard String type was loaded.
	EARLY_STRING = 0x04,
	// The GCObject is never collected. Until the program ends.
	// Use with caution.
	IMMORTAL = 0x08,
};
ENUM_OPS(GCOFlags, uint32_t);

#define GCO_SIZE    ALIGN_TO(sizeof(::GCObject),8)

// These GCO flags are always in the range 1–3
#define GCO_COLLECT(ccm) ((::GCOFlags)((ccm) + 1))
#define GCO_PROCESS(ccm) ((::GCOFlags)(((ccm) + 1) % 3 + 1))
#define GCO_KEEP(ccm)    ((::GCOFlags)(((ccm) + 2) % 3 + 1))

// The maximum amount of data that can be allocated before the GC kicks in.
// Objects larger than GC_LARGE_OBJECT_SIZE only contribute GC_LARGE_OBJECT_SIZE bytes
// to the debt, because they are unlikely to be short-lived objects.
#define GC_MAX_DEBT           1048576 // = 1 MB
#define GC_LARGE_OBJECT_SIZE  87040   // = 85 kB

typedef struct GCObject_S GCObject;
typedef struct GCObject_S
{
	GCOFlags flags; // Collection flag
	size_t size; // The size of the GCObject + fields.

	GCObject *prev; // Pointer to the previous GC object in the object's list (collect, process or keep).
	GCObject *next; // Pointer to the next GC object in the object's list.

	// A flag that is set while a thread is reading from or writing to
	// a field of this instance. No other threads can read from or write
	// to any field of the instance while this flag is set. This is to
	// prevent race conditions, as Value cannot be read or written atomically.
	std::atomic_flag fieldAccessFlag;

	Type *type;

	// The first field of the Value immediately follows the type;
	// this is the base of the Value's fields/custom pointer.

	inline void Mark(GCOFlags mark)
	{
		flags = mark | flags & ~GCOFlags::MARK;
	}

	inline uint8_t *InstanceBase()
	{
		return (uint8_t*)this + GCO_SIZE;
	}
	inline uint8_t *InstanceBase(Type *type)
	{
		return (uint8_t*)this + GCO_SIZE + type->fieldsOffset;
	}
	inline Value *FieldsBase()
	{
		return (Value*)((char*)this + GCO_SIZE);
	}
	inline Value *FieldsBase(Type *type)
	{
		return (Value*)((char*)this + GCO_SIZE + type->fieldsOffset);
	}

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

	inline static GCObject *FromInst(void *inst)
	{
		return reinterpret_cast<GCObject*>((char*)inst - GCO_SIZE);
	}
	inline static GCObject *FromValue(Value *value)
	{
		return FromInst(value->instance);
	}
} GCObject;

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
		accessFlag.clear(std::memory_order_release);
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

	static const size_t BLOCK_SIZE = 64;
	StaticRef values[BLOCK_SIZE];

	inline StaticRefBlock() : next(nullptr), count(0) { }
	inline StaticRefBlock(StaticRefBlock *next) : next(next), count(0) { }
};

class GC
{
private:
	bool isRunning;

	GCObject *collectBase;
	GCObject *processBase;
	GCObject *keepBase;

	// The current bit pattern used for marking an object as "collect".
	// This changes every GC cycle.
	int currentCollectMark;
	// The number of new bytes added to the GC since the last collection.
	size_t debt;
	// The total number of allocated bytes the GC knows about.
	size_t totalSize;

	uint32_t collectCount;

	StringTable strings;
	StaticRefBlock *staticRefs;

	inline void MakeImmortal(GCObject *gco)
	{
		gco->flags |= GCOFlags::IMMORTAL;
	}

	void *InternalAlloc(size_t size);
	void InternalRelease(GCObject *gco);

public:
	// Initializes the garbage collector.
	static void Init();
	// Unloads the garbage collector.
	static void Unload();

	GC();
	~GC();

	// Determines whether a particular Value should be processed.
	// A Value should be processed if:
	//   1. Its type is not null.
	//   2. Its type is not PRIMITIVE.
	//   3. It is not a string with the flag STATIC (no associated GCObject).
	//   4. Its GCObject* is marked GCO_COLLECT.
	// NOTE: This function is only called for /reachable/ Values.
	inline bool ShouldProcess(Value *val)
	{
		if (val->type == nullptr || (val->type->flags & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE)
			return false;

		if (val->type == VM::vm->types.String &&
			(val->common.string->flags & StringFlags::STATIC) == StringFlags::STATIC)
			return false;

		return (GCObject::FromValue(val)->flags & GCOFlags::MARK) == GCO_COLLECT(currentCollectMark);
	}

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

	void Release(GCObject *gco);

	void Collect(Thread *const thread);

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

	inline void Process(GCObject *gco)
	{
		// Must move from collect to process.
		assert((gco->flags & GCOFlags::MARK) == GCO_COLLECT(currentCollectMark));

		gco->RemoveFromList(&collectBase);
		if ((gco->flags & GCOFlags::EARLY_STRING) == GCOFlags::NONE &&
			gco->type->size > 0 /*|| gco->type->fieldsOffset > 0*/) // may have fields
		{
			gco->InsertIntoList(&processBase);
			gco->Mark(GCO_PROCESS(currentCollectMark));
		}
		else // no chance of instance fields, so nothing to process
		{
			gco->InsertIntoList(&keepBase);
			gco->Mark(GCO_KEEP(currentCollectMark));
		}
	}
	inline void Keep(GCObject *gco)
	{
		// Must move from process to keep, or keep an immortal object.
		assert((gco->flags & GCOFlags::MARK) == GCO_PROCESS(currentCollectMark) ||
			(gco->flags & GCOFlags::IMMORTAL) == GCOFlags::IMMORTAL);

		gco->RemoveFromList(&processBase);
		gco->InsertIntoList(&keepBase);
		gco->Mark(GCO_KEEP(currentCollectMark));
	}

	void MarkRootSet();

	inline void TryProcess(Value *value)
	{
		if (ShouldProcess(value))
			Process(GCObject::FromValue(value));
	}
	inline void TryProcessString(String *str)
	{
		if ((str->flags & StringFlags::STATIC) == StringFlags::NONE &&
			(GCObject::FromInst(str)->flags & GCOFlags::MARK) == GCO_COLLECT(currentCollectMark))
			Process(GCObject::FromInst(str));
	}

	void ProcessObjectAndFields(GCObject *gco);
	void ProcessCustomFields(Type *type, GCObject *gco);
	void ProcessHash(HashInst *hash);
	inline void ProcessFields(unsigned int fieldCount, Value fields[])
	{
		for (unsigned int i = 0; i < fieldCount; i++)
			// If the object is marked GCO_KEEP, we're done processing it.
			// If it's marked GCO_PROCESS, it'll be processed eventually.
			// Otherwise, mark it for processing!
			TryProcess(fields + i);
	}

	static GC *gc;

	friend class VM;
};

#endif // VM__GC_INTERNAL_H