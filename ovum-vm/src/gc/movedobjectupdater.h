#pragma once

#include "../vm.h"

// As part of a GC cycle, objects in generation 0 are moved out into generation
// 1, which entails actually physically moving the data in memory. This does of
// course mean that every single reference to each moved object is invalidated,
// so we need some way of updating these references.
//
// That's what MovedObjectUpdater is for.
//
// MovedObjectUpdater walks through the root set as well as one level of the
// object graph, updating all references to moved gen0 objects.
//
// NOTE: This class assumes live objects have been located beforehand (using
// LiveObjectFinder), and that gen0 objects have been moved to gen1 (using the
// method GC::MoveGen0Survivors()). Failure to do these things first absolutely
// leads to undesired and weird behaviour.

namespace ovum
{

class MovedObjectUpdater
{
public:
	MovedObjectUpdater(GC *gc, GCObject **keepList);

	void UpdateMovedObjects(GCObject *list);

	// RootSetVisitor methods

	void VisitRootValue(Value *value);

	void VisitRootLocalValue(Value *const value);

	void VisitRootString(String *str);

	bool EnterStaticRefBlock(StaticRefBlock *const refs);

	void LeaveStaticRefBlock(StaticRefBlock *const refs);

	// ObjectGraphVisitor methods

	bool EnterObject(GCObject *gco);

	void LeaveObject(GCObject *gco);

	void VisitFieldValue(Value *value);

	void VisitFieldString(String **str);

	void VisitFieldArray(void **arrayBase);

private:
	GC *gc;

	// The "keep" list, which receives all live, non-pinned objects.
	// Objects are moved to this list as they are processed.
	//
	// This points to LiveObjectFinder::keepList.
	GCObject **keepList;

	// Cached for speediness.
	Type *stringType;

	// Tries to find a value's GCObject. If the value does not have
	// an associated GCObject, the result is null.
	GCObject *ValueToGco(Value *value);

	void TryUpdateValue(Value *value);

	void TryUpdateString(String **str);
};

} // namespace ovum