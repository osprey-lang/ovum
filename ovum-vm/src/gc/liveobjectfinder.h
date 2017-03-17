#pragma once

#include "../vm.h"
#include "gcobject.h"

// The primary purpose of the LiveObjectFinder is to find live objects, exactly
// as you might expect from the name. The secondary purpose: to categorise live
// objects by generation. Let's discuss both.
//
// In order to find live objects, the class implements RootSetVisitor as well as
// ObjectGraphVisitor. The RootSetWalker only visits live objects, and then we
// can visit all of their members recursively, through ObjectGraphWalker. As we
// locate survivors, we add them to the gray set (the "process" list), so that
// the next ObjectGraphWalker iteration can process their members. As we process
// each object in the gray set, we move it to the black set, which contains all
// survivors whose members have been examined. Pretty normal tri-state garbage
// collector, in other words.
//
// If we can determine that an object cannot possibly contain any references of
// its own, we can move it directly to the black set. Hence we can save a tiny
// bit of time by not even trying to process its members.
//
// However, as a survivor is added to the black set, we want to categorise it
// based on certain generational characteristics. Since gen0 objects are moved
// to gen1 before the garbage collection cycle concludes, we need to classify
// survivors into one of the following groups, prioritised as shown:
//
// 1. Survivors from generation 0.
// 2. Survivors with references to generation 0 objects.
// 3. All other survivors.
//
// We do this so as to spend minimal time updating references to gen0 objects,
// after moving them to gen1. The rationale behind the three groups is this:
//
// 1. Gen0 objects are most likely to contain references to other gen0 objects,
//    since objects tend to be created together in an "inside out" fashion. In
//    practice, we will need to update many references of gen0 objects, so the
//    performance impact of examining all of these objects is minimal.
// 2. Survivors with gen0 references need to have those references updated as
//    soon as gen0 objects have been moved. We can't really get around having
//    to examine these object's members.
// 3. The remaining survivors have no gen0 references. We don't have to do a
//    single thing about them. These will be the regular "keep" list.
//
// Note that not ALL gen0 objects are moved to gen1: in particular, pinned gen0
// objects CANNOT be moved (that's the point of pinning). These are added back
// to the GC's pinnedList as they are found. See GC::AddPinnedObject() for more
// details.
//
// The LiveObjectFinder also keeps track of the total size of gen1 survivors, as
// this value is later used to determine whether to collect gen1 garbage.
//
// Technically, large objects (meaning primarily sizable GC-managed arrays) do
// not belong to generation 0 or 1, as they are in a wholly separate heap. But
// for the purposes of this discussion, since they don't move, we will treat
// them like gen1 objects.

namespace ovum
{

class LiveObjectFinder
{
public:
	LiveObjectFinder(GC *gc);

	void FindLiveObjects();

	// RootSetVisitor methods
	// These are only called during the initial marking phase, where we
	// find all live objects in the root set.

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
	// Cached values for maximum performance
	GCOFlags currentWhite;
	GCOFlags currentBlack;
	Type *stringType;

	// If the current object or static ref block contains references to
	// generation 0, this member is set to true. It is reset to false
	// upon entering an object or static ref block.
	// We track this for static ref blocks so we can skip entire blocks
	// if no value in the block is in generation 0. For performance, is
	// the basic idea.
	bool hasGen0Refs;

	// The total size of survivors from generation 1.
	size_t gen1SurvivorSize;

	// The gray set; that is, objects whose members are to be visited and
	// processed.
	GCObject *processList;

	// The black list. Initially containing only gen1 survivors without
	// gen0 references, later filled in with other survivors.
	GCObject *keepList;

	// Survivors from generation 0. These will be moved to generation 1,
	// except for pinned objects.
	GCObject *survivorsFromGen0;

	// Gen1 survivors with gen0 references. These will have their members
	// examined after gen0 survivors are moved.
	GCObject *survivorsWithGen0Refs;

	// Makes a value gray, if it should be made gray. Side effect: sets
	// hasGen0Refs to true if the value is in gen0.
	void TryGrayValue(Value *value);

	// Makes a string gray, if it should be made gray. Side effect: sets
	// hasGen0Refs to true if the string is in gen0.
	void TryGrayString(String *str);

	// Determines whether a value should be made gray. Side effect: sets
	// hasGen0Refs to true if the value is in gen0 (even if the method
	// returns false).
	//
	// A value should be grayed if the following conditions are met:
	//
	// * It is not null;
	// * It is not of a primitive type;
	// * It is not a static string (no associated GCObject); and
	// * Its GCObject is white.
	//
	// Note: This method is only called for reachable values. Unreachable
	// values will never be visited, so will never be grayed.
	bool ShouldGrayValue(Value *value);

	// Makes an object gray.
	void GrayObject(GCObject *gco);

	// Adds the object to an appropriate survivor list.
	void AddSurvivor(GCObject *gco);

	friend class GC;
};

} // namespace ovum
