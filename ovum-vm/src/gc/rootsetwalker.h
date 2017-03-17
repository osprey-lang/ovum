#pragma once

#include "../vm.h"
#include "gc.h"
#include "staticref.h"
#include "../ee/thread.h"
#include "../ee/vm.h"
#include "../object/method.h"
#include "../module/module.h"
#include "../module/modulepool.h"
#include "../debug/debugsymbols.h"

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
// * The current error being handled (if any), as well as any error saved by a
//   finally or fault clause (see Thread::ErrorStack).
//
// Note that interned strings are NOT in the root set: they can be deallocated
// like any other value (and subsequently removed from the intern table).
//
// In order to walk the root set, you must implement the methods of the prototype
// RootSetVisitor. See the documentation of each visit method for more details.
// The visitor must manage any state required while processing the root set.
//
// A class can safely implement both RootSetVisitor and ObjectGraphVisitor. The
// method names do not overlap.

namespace ovum
{

// NOTE: This class is a prototype class! Do not extend it; merely implement the
// methods detailed below, and use the concrete implementation when you call methods
// RootSetWalker.
class RootSetVisitor
{
public:
	// Visits a single value in the root set.
	//
	// The value is guaranteed NOT to be a local value (see VisitRootLocalValue()
	// for details).
	void VisitRootValue(Value *value);

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
	void VisitRootLocalValue(Value *const value);

	// Visits a single string value in the root set. Root set strings are always
	// allocated directly into generation 1, so never need to be moved by the GC.
	//
	// These strings must nevertheless be visited in order to mark the underlying
	// GCObject as alive.
	void VisitRootString(String *str);

	// Enters a static reference block. This is called before the values inside
	// a StaticRefBlock are visited. If the method return false, no values in
	// that block visited, and LeaveStaticBlock() is not subsequently called.
	//
	// No more than one static reference block will be entered at any given time.
	// That is, static reference blocks are never entered recursively.
	bool EnterStaticRefBlock(StaticRefBlock *const refs);

	// Leaves the current (last entered) static reference block.
	//
	// When EnterStaticRefBlock() returns false for a block, this method is not
	// called for that block.
	//
	// No more than one static reference block will be entered at any given time.
	// That is, static reference blocks are never entered recursively.
	void LeaveStaticRefBlock(StaticRefBlock *const refs);
};

template<class Visitor>
class RootSetWalker
{
public:
	inline RootSetWalker(GC *gc) :
		gc(gc),
		vm(gc->GetVM())
	{ }

	void VisitRootSet(Visitor &visitor)
	{
		VisitThread(visitor, vm->mainThread.get());

		VisitModulePool(visitor, vm->GetModulePool());

		VisitStaticRefs(visitor, gc->staticRefs.get());
	}

private:
	GC *gc;
	VM *vm;

	void VisitThread(Visitor &visitor, Thread *const thread)
	{
		VisitStackFrames(visitor, thread);

		visitor.VisitRootValue(&thread->currentError);

		if (thread->errorStack != nullptr)
		{
			auto *errorStack = thread->errorStack;
			do
			{
				visitor.VisitRootValue(&errorStack->error);
				errorStack = errorStack->prev;
			} while (errorStack != nullptr);
		}
	}

	void VisitStackFrames(Visitor &visitor, Thread *const thread)
	{
		StackFrame *frame = thread->currentFrame;

		// The very first stack frame on the thread has a null method.
		// It's essentially a "fake" stack frame, which receives only the
		// arguments for the thread's startup method.
		while (frame != nullptr && frame->method != nullptr)
		{
			MethodOverload *method = frame->method;

			ovlocals_t paramCount = method->GetEffectiveParamCount();
			if (paramCount != 0)
				VisitLocalValues(visitor, paramCount, reinterpret_cast<Value*>(frame)-paramCount);

			// By design, local variables and evaluation stack values are adjacent
			// in memory, so it's safe to read from them as if they were the same
			// array of values.
			ovlocals_t localCount = method->locals + frame->stackCount;
			if (localCount != 0)
				VisitLocalValues(visitor, localCount, frame->Locals());

			frame = frame->prevFrame;
		}
	}

	void VisitLocalValues(Visitor &visitor, ovlocals_t count, Value *values)
	{
		for (ovlocals_t i = 0; i < count; i++)
			visitor.VisitRootLocalValue(values + i);
	}

	void VisitModulePool(Visitor &visitor, ModulePool *pool)
	{
		int moduleCount = pool->GetLength();
		for (int i = 0; i < moduleCount; i++)
		{
			Module *module = pool->Get(i);
			VisitModule(visitor, module);
		}
	}

	void VisitModule(Visitor &visitor, Module *module)
	{
		visitor.VisitRootString(module->GetName());

		size_t stringCount = module->strings.GetLength();
		for (size_t i = 0; i < stringCount; i++)
			visitor.VisitRootString(module->strings[i]);

		if (module->debugData)
			VisitDebugData(visitor, module->debugData.get());
	}

	void VisitDebugData(Visitor &visitor, debug::ModuleDebugData *debug)
	{
		size_t fileCount = debug->fileCount;
		for (size_t i = 0; i < fileCount; i++)
			visitor.VisitRootString(debug->files[i].fileName);
	}

	void VisitStaticRefs(Visitor &visitor, StaticRefBlock *refs)
	{
		while (refs != nullptr)
		{
			if (visitor.EnterStaticRefBlock(refs))
			{
				size_t count = refs->count;
				for (size_t i = 0; i < count; i++)
					visitor.VisitRootValue(refs->values[i].GetValuePointer());
				visitor.LeaveStaticRefBlock(refs);
			}

			refs = refs->next.get();
		}
	}
};

} // namespace ovum
