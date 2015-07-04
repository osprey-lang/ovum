#ifndef VM__GCOBJECT_H
#define VM__GCOBJECT_H

#include "../vm.h"
#include "../threading/sync.h"

namespace ovum
{

enum class GCOFlags : uint32_t
{
	NONE          = 0x0000,
	// The mark occupies the lowest two bits.
	// Objects are marked according to what list they belong to:
	// the Collect list uses GC::currentCollectMark, Process
	// uses GCOFlags::PROCESS (invariant), and the Keep list uses
	// GC::currentKeepMark.
	PROCESS       = 0x0002, // The object is in the Process list.
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

	// The GCObject is a GC-managed array. If GCObject::type is
	// GC_VALUE_ARRAY, then the array contains Values. Otherwise,
	// we have no idea what it contains.
	ARRAY         = 0x0200,
};
OVUM_ENUM_OPS(GCOFlags, uint32_t);

class GCObject
{
public:
	GCOFlags flags; // Collection flag
	size_t size; // The size of the GCObject + fields.

	uint32_t pinCount;
	uint32_t hashCode;

	GCObject *prev; // Pointer to the previous GC object in the object's list.
	GCObject *next; // Pointer to the next GC object in the object's list.

	// A lock that is entered while a thread is reading from or writing to
	// a field of this instance. No other threads can read from or write
	// to any field of the instance while this lock is held. This is to
	// prevent race conditions, as Value cannot be read or written atomically.
	SpinLock fieldAccessLock;

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

	inline bool IsEarlyString() const { return (flags & GCOFlags::EARLY_STRING) == GCOFlags::EARLY_STRING; }
	inline bool IsArray()       const { return (flags & GCOFlags::ARRAY) == GCOFlags::ARRAY; }
	inline bool IsPinned()      const { return (flags & GCOFlags::PINNED) == GCOFlags::PINNED; }
	inline bool HasGen0Refs()   const { return (flags & GCOFlags::HAS_GEN0_REFS) == GCOFlags::HAS_GEN0_REFS; }
	inline bool IsMoved()       const { return (flags & GCOFlags::MOVED) == GCOFlags::MOVED; }

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

static const size_t GCO_SIZE = OVUM_ALIGN_TO(sizeof(GCObject), 8);

} // namespace ovum

#endif // VM__GCOBJECT_H
