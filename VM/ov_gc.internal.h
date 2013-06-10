#pragma once

#ifndef VM__GC_INTERNAL_H
#define VM__GC_INTERNAL_H

#include <cassert>
#include "ov_vm.internal.h"

TYPED_ENUM(GCOFlags, uint8_t)
{
	GCO_NONE  = 0x00,
	// The mark occupies the lowest two bits.
	// Collectible objects are marked with currentCollectMark,
	// which changes each cycle.
	// To obtain the marks of the current cycle, use these macros:
	//   GCO_COLLECT(currentCollectMark)
	//   GCO_PROCESS(currentCollectMark)
	//   GCO_KEEP(currentCollectMark)
	GCO_MARK  = 0x03, // Mask for extracting the mark.
};

#define MARK_GCO(gco,mk) ((gco)->flags = (::GCOFlags)((mk) | ((gco)->flags & ~GCO_MARK)))

#define GCO_COLLECT(ccm) ((::GCOFlags)(ccm))
#define GCO_PROCESS(ccm) ((::GCOFlags)(((ccm) + 1) % 3))
#define GCO_KEEP(ccm)    ((::GCOFlags)(((ccm) + 2) % 3))

#define GCO_CLEAR_LINKS(gco) ((gco)->next = (gco)->prev = nullptr)

#define GCO_INSTANCE_BASE(gco) ((uint8_t*)((gco) + 1))
#define GCO_FIELDS_BASE(gco)   ((::Value*)((gco) + 1))

// The maximum amount of data that can be allocated before the GC kicks in.
// Objects larger than GC_LARGE_OBJECT_SIZE only contribute GC_LARGE_OBJECT_SIZE bytes
// to the debt, because they are unlikely to be short-lived objects.
#define GC_MAX_DEBT           524288 // = 512 kB
#define GC_LARGE_OBJECT_SIZE  87040  // = 85 kB

typedef struct GCObject_S GCObject;
typedef struct GCObject_S
{
	GCOFlags flags; // Collection flag
	size_t size; // The size of the GCObject + fields.

	GCObject *prev; // Pointer to the previous GC object in the object's list (collect, process or keep).
	GCObject *next; // Pointer to the next GC object in the object's list.

	const Type *type;

	// The first field of the Value immediately follows the type;
	// this is the base of the Value's fields/custom pointer.
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

typedef struct GCState_S
{
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
} GCState;
extern GCState gcState;

// Initializes the garbage collector.
void GC_Init();

// Recovers a GCObject* from an instance pointer.
#define GCO_FROM_INST(inst)  (reinterpret_cast<::GCObject*>(inst) - 1)
// Recovers a GCObject* from a Value.
#define GCO_FROM_VALUE(val)  GCO_FROM_INST((val).instance)
// Gets an instance pointer from a GCObject*, for a particular type.
#define INST_FROM_GCO(gco,t) ((uint8_t*)((gco) + 1) + (t)->fieldsOffset)


// Determines whether a particular Value should be processed.
// A Value should be processed if:
//   1. Its type does not have the TYPE_PRIMITIVE flag set.
//     1a. Its type is not null.
//     1b. It's not a string marked STR_STATIC (as they do not
//         have an associated GCObject).
//   2. Its GCObject* is marked GCO_COLLECT.
// NOTE: This function is only called for /reachable/ Values.
inline bool GC_ShouldProcess(Value val)
{
	return val.type && !(_Tp(val.type)->flags & TYPE_PRIMITIVE) &&
		(val.type != stdTypes.String || !(val.common.string->flags & STR_STATIC)) &&
		GCO_FROM_VALUE(val)->flags & GCO_COLLECT(gcState.currentCollectMark);
}

void GC_Alloc(Thread *const thread, const Type *type, size_t size, GCObject **output);
inline void GC_Alloc(Thread *const thread, const Type *type, size_t size, Value *output)
{
	GCObject *gco;
	GC_Alloc(thread, type, size, &gco);
	output->type = type;
	output->instance = GCO_INSTANCE_BASE(gco);
}


void GC_Release(Thread *const thread, GCObject *gco);

void GC_Collect(Thread *const thread);

// Removes a GCObject from its associated linked list.
// This should always be called before calling InsertIntoList, which
// does not automatically call this method for performance reasons.
//
// 'list' is the base of the list, and is only modified if gco->next
// and gco->prev are both null (that is, if the object is the only
// object in the list).
//
// NOTE: also for performance reasons, this code does NOT set gco's
// next and prev fields to null. RemoveFromList will be called mostly
// before calling InsertIntoList, which writes to those fields.
// If you need these fields to be null, you must set them yourself.
// Use the GCO_CLEAR_LINKS(gco) macro for this.
inline void GC_RemoveFromList(GCObject *gco, GCObject **list)
{
	GCObject *prev = gco->prev;
	GCObject *next = gco->next;
	// This code maintains two important facts:
	//   1. If gco->prev == nullptr (that is, gco is the first object in the list),
	//      then gco->next->prev will also be null.
	//   2. If gco->next == nullptr (that is, it's the last object in the list),
	//      then gco->prev->next will also be null.

	if (gco == *list)
		*list = next;

	if (!prev && !next)
	{
		*list = nullptr;
	}
	else
	{
		// Before removal:
		// prev  <->  gco  <->  next
		// After removal:
		// prev  <->  next
		if (prev) prev->next = next;
		if (next) next->prev = prev;
	}
}

// Inserts a GCObject into a linked list.
// The parameter 'list' points to the first object in the list.
inline void GC_InsertIntoList(GCObject *gco, GCObject **list)
{
	// Before insertion:
	// nullptr  <--  *list  <->  (*list)->next
	// After insertion:
	// nullptr  <--  gco  <->  *list  <->  (*list)->next
	gco->prev = nullptr;  // gco is the first value, so it has nothing prior to it.
	gco->next = *list; // The next value is the current base.
	if (*list)
		(*list)->prev = gco;
	*list = gco;       // And then we update the base of the list!
}

inline void GC_Process(GCObject *gco)
{
	// Must move from collect to process.
	assert(gco->flags & GCO_COLLECT(gcState.currentCollectMark));

	GC_RemoveFromList(gco, &gcState.collectBase);
	GC_InsertIntoList(gco, &gcState.processBase);
	MARK_GCO(gco, GCO_PROCESS(gcState.currentCollectMark));
}

inline void GC_Keep(GCObject *gco)
{
	// Must move from process to keep.
	assert(gco->flags & GCO_PROCESS(gcState.currentCollectMark));

	GC_RemoveFromList(gco, &gcState.processBase);
	GC_InsertIntoList(gco, &gcState.keepBase);
	MARK_GCO(gco, GCO_KEEP(gcState.currentCollectMark));
}


void GC_MarkRootSet();


void GC_ProcessObjectAndFields(GCObject *gco);
void GC_ProcessCustomFields(const Type *type, GCObject *gco);
void GC_ProcessHash(HashInst *hash);
inline void GC_ProcessFields(unsigned int fieldCount, Value fields[])
{
	for (unsigned int i = 0; i < fieldCount; i++)
	{
		Value field = fields[i];
		// If the object is marked GCO_KEEP, we're done processing it.
		// If it's marked GCO_PROCESS, it'll be processed eventually.
		// Otherwise, mark it for processing!
		if (GC_ShouldProcess(field))
			GC_Process(GCO_FROM_VALUE(field));
	}
}

#endif // VM__GC_INTERNAL_H