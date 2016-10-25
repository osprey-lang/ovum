#pragma once

#include "../vm.h"

// The RootSetWalker class, as the name implies, walks the so-called root set.
// The root set comprises values that are always guaranteed to be reachable;
// that is, values that are definitely alive. Currently the root set consists
// of the following:
//
// * Local values in every stack frame on every managed thread; that is, local
//   variables, evaluation stack values, and method arguments.
// * Values in static fields. These values remain alive for the lifetime of the
//   VM.
// * An assortment of string values, such as strings contained in modules, and
//   the file names attached to debug symbols. These are managed by the GC, so
//   the root set must include them.
//
// Note that interned strings are NOT in the root set: they can be deallocated
// like any other value (and subsequently removed from the intern table).
//
// In order to walk the root set, you must implement a RootSetVisitor, which is
// a pure virtual (abstract) class with a variety of Visit* methods. See the
// documentation of each visit method for more details. The visitor must manage
// any state required while processing the root set.
//
// A class can safely implement both RootSetVisitor and ObjectGraphVisitor. The
// method names do not overlap.

namespace ovum
{

class RootSetVisitor
{
public:
	// Visits a single value in the root set.
	//
	// The value is guaranteed NOT to be a local value (see VisitRootLocalValue()
	// for details).
	virtual void VisitRootValue(Value *value) = 0;

	// Visits a single local value in the root set. A local value is one of the
	// following:
	//
	// * A local variable;
	// * A value on the evaluation stack; or
	// * A method argument.
	//
	// NOTE: This method is separate from VisitRootValue() because local values
	// can be references. You must be prepared to handle references when you
	// accept values through this method.
	virtual void VisitRootLocalValue(Value *const value) = 0;

	// Visits a single string value in the root set. These strings are always
	// allocated directly into generation 1, so never need to be moved by the GC.
	//
	// These strings must nevertheless be visited in order to mark the underlying
	// GCObject as alive.
	virtual void VisitRootString(String *str) = 0;

	// Enters a static reference block. This is called before the values inside
	// a StaticRefBlock are visited. If the method return false, no values in
	// that block visited, and LeaveStaticBlock() is not subsequently called.
	//
	// No more than one static reference block will be entered at any given time.
	// That is, static reference blocks are never entered recursively.
	virtual bool EnterStaticRefBlock(StaticRefBlock *const refs) = 0;

	// Leaves the current (last entered) static reference block.
	//
	// When EnterStaticRefBlock() returns false for a block, this method is not
	// called for that block.
	//
	// No more than one static reference block will be entered at any given time.
	// That is, static reference blocks are never entered recursively.
	virtual void LeaveStaticRefBlock() = 0;
};

class RootSetWalker
{
public:
	RootSetWalker(GC *gc);

	void VisitRootSet(RootSetVisitor &visitor);

private:
	GC *gc;
	VM *vm;

	void VisitThread(RootSetVisitor &visitor, Thread *const thread);

	void VisitStackFrames(RootSetVisitor &visitor, Thread *const thread);

	void VisitLocalValues(RootSetVisitor &visitor, ovlocals_t count, Value *values);

	void VisitModulePool(RootSetVisitor &visitor, ModulePool *pool);

	void VisitModule(RootSetVisitor &visitor, Module *module);

	void VisitDebugData(RootSetVisitor &visitor, debug::ModuleDebugData *debug);

	void VisitStaticRefs(RootSetVisitor &visitor, StaticRefBlock *refs);
};

} // namespace ovum
