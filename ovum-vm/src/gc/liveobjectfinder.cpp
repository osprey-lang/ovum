#include "liveobjectfinder.h"
#include "gc.h"
#include "staticref.h"
#include "../object/value.h"
#include "../ee/vm.h"
#include "../object/type.h"

namespace ovum
{

LiveObjectFinder::LiveObjectFinder(GC *gc) :
	gc(gc),
	hasGen0Refs(false),
	gen1SurvivorSize(0),
	processList(nullptr),
	keepList(nullptr),
	survivorsFromGen0(nullptr),
	survivorsWithGen0Refs(nullptr)
{
	currentWhite = gc->currentWhite;
	currentBlack = gc->currentBlack;
	stringType = gc->GetVM()->types.String;
}

void LiveObjectFinder::FindLiveObjects()
{
	// Every object that can be reached from the root set is guaranteed
	// to be alive. Let's start by graying all of those objects, and add
	// all appropriate objects to the processList.
	RootSetWalker walker(this->gc);
	walker.VisitRootSet(*this);

	// Now we can start processing known survivors. We loop through each
	// object in processList, add their field references to the start of
	// that list, and repeat until processList is empty.
	while (processList != nullptr)
	{
		ObjectGraphWalker::VisitObjectList(*this, processList);
	}
	OVUM_ASSERT(processList == nullptr);

	// Now we have found all survivors, and grouped them into the correct
	// survivor lists, which means we're done!
}

void LiveObjectFinder::TryGrayValue(Value *value)
{
	if (ShouldGrayValue(value))
		GrayObject(GCObject::FromValue(value));
}

void LiveObjectFinder::TryGrayString(String *str)
{
	// If the string is not static, it has an associated GCObject.
	if ((str->flags & StringFlags::STATIC) == StringFlags::NONE)
	{
		GCObject *gco = GCObject::FromInst(str);

		// If the string belongs to generation 0, mark whatever referred
		// to it as having gen0 references:
		if ((gco->flags & GCOFlags::GEN_0) == GCOFlags::GEN_0)
			hasGen0Refs = true;

		// If the GCObject is white, we need to gray it.
		if (gco->GetColor() == currentWhite)
			GrayObject(gco);
	}
}

bool LiveObjectFinder::ShouldGrayValue(Value *value)
{
	// A value should be grayed if the following conditions are met:
	//
	// * It is not null;
	// * It is not of a primitive type;
	// * It is not a static string (no associated GCObject); and
	// * Its GCObject is white.
	if (value->type == nullptr || value->type->IsPrimitive())
		return false;

	if (value->type == stringType &&
		(value->v.string->flags & StringFlags::STATIC) != StringFlags::NONE)
		return false;

	// The null value, primitive values, and static strings do not have
	// associated GCObjects. Since we have ruled out those possibilities
	// now, we can safely retrieve a GCObject (or at least, its flags):
	GCOFlags flags = GCObject::FromValue(value)->flags;

	// If the value is a non-pinned gen0 object, its address will have
	// to be updated once moved to gen1. Mark whatever referred to this
	// value as having gen0 references:
	if ((flags & GCOFlags::GEN_0) == GCOFlags::GEN_0 &&
		(flags & GCOFlags::PINNED) == GCOFlags::NONE)
	{
		hasGen0Refs = true;
	}

	return (flags & GCOFlags::COLOR) == currentWhite;
}

void LiveObjectFinder::GrayObject(GCObject *gco)
{
	// We can only move to gray from white.
	OVUM_ASSERT(gco->GetColor() == currentWhite);

	Type *type = gco->type;
	OVUM_ASSERT(
		// If gco is an early sting (that is, a string allocated before aves.String
		// had been loaded), its type must be null.
		gco->IsEarlyString() ? type == nullptr :
		// If gco is a GC-managed array, its type must be null or GC_VALUE_ARRAY.
		gco->IsArray() ? type == nullptr || (uintptr_t)type == GC::GC_VALUE_ARRAY :
		// Otherwise, the type must be non-null.
		type != nullptr
	);

	// Now let's move the GCObject to the correct list. We'll start by removing
	// it from its current list:
	gco->RemoveFromList(&gc->collectList);
	// Note that pinned objects are moved to collectList before any objects are
	// examined, and are put back in pinnedList only after we've located all the
	// survivors.

	bool couldContainFields =
		// If the type is null, the value is an early string or a GC-managed non-
		// Value array. In that case, the type cannot contain any Value fields.
		type != nullptr &&
		// If the type is not null, then:
		(
			// If it's a Value array, it almost certainly contains managed data;
			(uintptr_t)type == GC::GC_VALUE_ARRAY ||
			// Or if it's flagged as containing managed refs, it probably does.
			type->HasManagedRefs()
		);

	if (couldContainFields)
	{
		// If the value could contain managed Value fields, we have to make it
		// gray so it can be examined in the next pass.
		gco->InsertIntoList(&processList);
		gco->SetColor(GCOFlags::GRAY);
	}
	else
	{
		// No chance of instance fields, so nothing to process. Move it directly
		// to the black set.
		AddSurvivor(gco);
		gco->SetColor(currentBlack);
	}
}

void LiveObjectFinder::AddSurvivor(GCObject *gco)
{
	GCObject **list = nullptr;
	if ((gco->flags & GCOFlags::GEN_0) == GCOFlags::GEN_0)
	{
		list = &survivorsFromGen0;
	}
	else
	{
		if (gco->HasGen0Refs())
			list = &survivorsWithGen0Refs;
		else
			list = &keepList;

		// We have to keep track of the total gen1 survivor size too.
		if ((gco->flags & GCOFlags::GEN_1) == GCOFlags::GEN_1)
			gen1SurvivorSize += gco->size;
	}

	gco->InsertIntoList(list);
}

void LiveObjectFinder::VisitRootValue(Value *value)
{
	TryGrayValue(value);
}

void LiveObjectFinder::VisitRootLocalValue(Value *const value)
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
		// field, the reference may hold the only remaining pointer
		// to the particular instance.
		if (type != LOCAL_REFERENCE &&
			type != STATIC_REFERENCE)
		{
			// In an instance field reference, Value::type stores the
			// bitwise inverse of the byte offset of the instance field
			// from the base of the GCObject. Value::v::reference is a
			// pointer to the GCObject. We only want the GCObject.
			GCObject *gco = reinterpret_cast<GCObject*>(value->v.reference);
			if (gco->GetColor() == currentWhite)
				GrayObject(gco);
		}
	}
	else
	{
		// If it's not a reference, treat it like any other value.
		TryGrayValue(value);
	}
}

void LiveObjectFinder::VisitRootString(String *str)
{
	TryGrayString(str);
}

bool LiveObjectFinder::EnterStaticRefBlock(StaticRefBlock *const refs)
{
	// Always enter static ref blocks during the initial root set
	// marking phase. We need to know where the gen0 refs are!
	hasGen0Refs = false;
	return true;
}

void LiveObjectFinder::LeaveStaticRefBlock(StaticRefBlock *const refs)
{
	refs->hasGen0Refs = hasGen0Refs;
}

bool LiveObjectFinder::EnterObject(GCObject *gco)
{
	// If an object gets here, it must be a gray object.
	OVUM_ASSERT(gco->GetColor() == GCOFlags::GRAY);

	// Make the object black immediately.
	gco->SetColor(currentBlack);

	hasGen0Refs = false;

	// If the object has been added to the processList, we know it
	// might have some instance fields. We'll want to examine them.
	return true;
}

void LiveObjectFinder::LeaveObject(GCObject *gco)
{
	if (hasGen0Refs)
		gco->flags |= GCOFlags::HAS_GEN0_REFS;

	// Now let's move this survivor to the appropriate survivor list.
	gco->RemoveFromList(&processList);
	AddSurvivor(gco);
}

void LiveObjectFinder::VisitFieldValue(Value *value)
{
	TryGrayValue(value);
}

void LiveObjectFinder::VisitFieldString(String **str)
{
	TryGrayString(*str);
}

void LiveObjectFinder::VisitFieldArray(void **arrayBase)
{
	// The base of the array is the base of the instance, from which
	// we can get a GCObject.
	GCObject *gco = GCObject::FromInst(*arrayBase);

	// If we have a non-pinned gen0 object, we have to set hasGen0Refs
	// to true.
	if ((gco->flags & GCOFlags::GEN_0) == GCOFlags::GEN_0 &&
		!gco->IsPinned())
		hasGen0Refs = true;

	// And if its white, we need to gray it.
	if (gco->GetColor() == currentWhite)
		GrayObject(gco);
}

} // namespace ovum
