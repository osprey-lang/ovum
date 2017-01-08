#include "movedobjectupdater.h"
#include "liveobjectfinder.h"
#include "gc.h"
#include "staticref.h"
#include "../ee/vm.h"
#include "../object/type.h"
#include "../object/value.h"

namespace ovum
{

MovedObjectUpdater::MovedObjectUpdater(GC *gc, GCObject **keepList) :
	gc(gc),
	keepList(keepList)
{
	stringType = gc->GetVM()->types.String;
}

void MovedObjectUpdater::UpdateMovedObjects(GCObject *list)
{
	RootSetWalker rootWalker(gc);
	rootWalker.VisitRootSet(*this);

	ObjectGraphWalker::VisitObjectList(*this, list);

	// We have to update the GC's pinnedList too
	ObjectGraphWalker::VisitObjectList(*this, gc->pinnedList);
}

GCObject *MovedObjectUpdater::ValueToGco(Value *value)
{
	if (value->type == nullptr ||
		value->type->IsPrimitive())
		return nullptr;

	if (value->type == stringType &&
		(value->v.string->flags & StringFlags::STATIC) != StringFlags::NONE)
		return nullptr;

	return GCObject::FromInst(value->v.instance);
}

void MovedObjectUpdater::TryUpdateValue(Value *value)
{
	GCObject *gco = ValueToGco(value);
	if (gco != nullptr && gco->IsMoved())
		value->v.instance = gco->newAddress->InstanceBase();
}

void MovedObjectUpdater::TryUpdateString(String **str)
{
	// Static strings have no associated GCObject, so nothing to update.
	if (((*str)->flags & StringFlags::STATIC) == StringFlags::NONE)
	{
		GCObject *gco = GCObject::FromInst(*str);
		if (gco->IsMoved())
			*str = reinterpret_cast<String*>(gco->newAddress->InstanceBase());
	}
}

void MovedObjectUpdater::VisitRootValue(Value *value)
{
	TryUpdateValue(value);
}

void MovedObjectUpdater::VisitRootLocalValue(Value *const value)
{
	// Local values differ from non-local values in one respect:
	// they may contain references. References are recognised by
	// having the least significant bit of the type set to 1.
	uintptr_t type = reinterpret_cast<uintptr_t>(value->type);
	if ((type & 1) == 1)
	{
		// We only need to look at references to instance fields.
		// Static fields and local variables are part of the root
		// set, so will be visited eventually. But with an instance
		// field, the instance itself may have moved.
		if (type != LOCAL_REFERENCE &&
			type != STATIC_REFERENCE)
		{
			// In an instance field reference, Value::type stores the
			// bitwise inverse of the byte offset of the instance field
			// from the base of the GCObject. Value::v::reference is a
			// pointer to the GCObject. We only want the GCObject.
			GCObject *gco = reinterpret_cast<GCObject*>(value->v.reference);
			if (gco->IsMoved())
				value->v.reference = gco->newAddress;
		}
	}
	else
	{
		TryUpdateValue(value);
	}
}

void MovedObjectUpdater::VisitRootString(String *str)
{
	// Root strings are always allocated in generation 1 directly, and
	// so should never require moving.
	OVUM_ASSERT(
		(str->flags & StringFlags::STATIC) == StringFlags::STATIC ||
		!GCObject::FromInst(str)->IsMoved()
	);
}

bool MovedObjectUpdater::EnterStaticRefBlock(StaticRefBlock *const refs)
{
	// We only need to examine the values in the static ref block if
	// any of them are in generation 0.
	return refs->hasGen0Refs;
}

void MovedObjectUpdater::LeaveStaticRefBlock(StaticRefBlock *const refs)
{
	// Reset for next cycle.
	refs->hasGen0Refs = false;
}

bool MovedObjectUpdater::EnterObject(GCObject *gco)
{
	// If the object is NOT pinned, move it to the "keep" list. Otherwise
	// leave it in GC::pinnedList, where it belongs.
	if (!gco->IsPinned())
		gco->InsertIntoList(keepList);

	// We only need to examine the object's references if any of them
	// are in generation 0.
	return gco->HasGen0Refs();
}

void MovedObjectUpdater::LeaveObject(GCObject *gco)
{
	// Reset for next cycle.
	gco->flags &= ~GCOFlags::HAS_GEN0_REFS;
}

void MovedObjectUpdater::VisitFieldValue(Value *value)
{
	TryUpdateValue(value);
}

void MovedObjectUpdater::VisitFieldString(String **str)
{
	TryUpdateString(str);
}

void MovedObjectUpdater::VisitFieldArray(void **arrayBase)
{
	// arrayBase contains a pointer to the GCObject's instance base.
	GCObject *gco = GCObject::FromInst(*arrayBase);
	if (gco->IsMoved())
		*arrayBase = gco->newAddress->InstanceBase();
}

} // namespace ovum
