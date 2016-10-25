#include "rootsetwalker.h"
#include "gc.h"
#include "staticref.h"
#include "../ee/thread.h"
#include "../ee/vm.h"
#include "../object/method.h"
#include "../module/module.h"
#include "../module/modulepool.h"
#include "../debug/debugsymbols.h"

namespace ovum
{

RootSetWalker::RootSetWalker(GC *gc) :
	gc(gc),
	vm(gc->GetVM())
{ }

void RootSetWalker::VisitRootSet(RootSetVisitor &visitor)
{
	VisitThread(visitor, vm->mainThread.get());

	VisitModulePool(visitor, vm->GetModulePool());

	VisitStaticRefs(visitor, gc->staticRefs.get());
}

void RootSetWalker::VisitThread(RootSetVisitor &visitor, Thread *const thread)
{
	VisitStackFrames(visitor, thread);

	visitor.VisitRootValue(&thread->currentError);
}

void RootSetWalker::VisitStackFrames(RootSetVisitor &visitor, Thread *const thread)
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

void RootSetWalker::VisitLocalValues(RootSetVisitor &visitor, ovlocals_t count, Value *values)
{
	for (ovlocals_t i = 0; i < count; i++)
		visitor.VisitRootLocalValue(values + i);
}

void RootSetWalker::VisitModulePool(RootSetVisitor &visitor, ModulePool *pool)
{
	int moduleCount = pool->GetLength();
	for (int i = 0; i < moduleCount; i++)
	{
		Module *module = pool->Get(i);
		VisitModule(visitor, module);
	}
}

void RootSetWalker::VisitModule(RootSetVisitor &visitor, Module *module)
{
	visitor.VisitRootString(module->GetName());

	int32_t stringCount = module->strings.GetLength();
	for (int32_t i = 0; i < stringCount; i++)
		visitor.VisitRootString(module->strings[i]);

	if (module->debugData)
		VisitDebugData(visitor, module->debugData.get());
}

void RootSetWalker::VisitDebugData(RootSetVisitor &visitor, debug::ModuleDebugData *debug)
{
	int32_t fileCount = debug->fileCount;
	for (int32_t i = 0; i < fileCount; i++)
		visitor.VisitRootString(debug->files[i].fileName);
}

void RootSetWalker::VisitStaticRefs(RootSetVisitor &visitor, StaticRefBlock *refs)
{
	while (refs != nullptr)
	{
		if (visitor.EnterStaticRefBlock(refs))
		{
			uint32_t count = refs->count;
			for (uint32_t i = 0; i < count; i++)
				visitor.VisitRootValue(refs->values[i].GetValuePointer());
			visitor.LeaveStaticRefBlock();
		}

		refs = refs->next.get();
	}
}

} // namespace ovum
