#pragma once

#include "../vm.h"
#include "rootsetwalker.h"
#include "objectgraphwalker.h"

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

class MovedObjectUpdater :
	private RootSetVisitor,
	private ObjectGraphVisitor
{
public:
	MovedObjectUpdater(GC *gc);

	void UpdateMovedObjects(GCObject *list);

private:
	GC *gc;

	// Cached for speediness.
	Type *stringType;

	// Tries to find a value's GCObject. If the value does not have
	// an associated GCObject, the result is null.
	GCObject *ValueToGco(Value *value);

	void TryUpdateValue(Value *value);

	void TryUpdateString(String **str);

	// RootSetVisitor methods

	virtual void VisitRootValue(Value *value);

	virtual void VisitRootLocalValue(Value *const value);

	virtual void VisitRootString(String *str);

	virtual bool EnterStaticRefBlock(StaticRefBlock *const refs);

	virtual void LeaveStaticRefBlock(StaticRefBlock *const refs);

	// ObjectGraphVisitor methods

	virtual bool EnterObject(GCObject *gco);

	virtual void LeaveObject(GCObject *gco);

	virtual void VisitFieldValue(Value *value);

	virtual void VisitFieldString(String **str);

	virtual void VisitFieldArray(void **arrayBase);
};

} // namespace ovum