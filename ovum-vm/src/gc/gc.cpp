#include "gc.h"
#include "gcobject.h"
#include "staticref.h"
#include "../ee/thread.h"
#include "../module/module.h"
#include "../module/modulepool.h"
#include "../object/type.h"
#include "../object/method.h"
#include "../object/value.h"
#include "../debug/debugsymbols.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ovum
{

namespace gc_strings
{
	LitString<47> _ObjectTooBig  = LitString<47>::FromCString("The size of the requested object was too large.");
	LitString<33> _CStringTooBig = LitString<33>::FromCString("GC_ConvertString: input too long.");

	String *ObjectTooBig  = _ObjectTooBig.AsString();
	String *CStringTooBig = _CStringTooBig.AsString();
}

int GC::Create(VM *owner, GC *&result)
{
	GC *gc = new(std::nothrow) GC(owner);
	if (!gc)
		return OVUM_ERROR_NO_MEMORY;
	if (!gc->InitializeHeaps())
		return OVUM_ERROR_NO_MEMORY;

	result = gc;
	RETURN_SUCCESS;
}

GC::GC(VM *owner) :
	collectList(nullptr), pinnedList(nullptr),
	currentCollectMark((GCOFlags)1),
	currentKeepMark((GCOFlags)3),
	gen1Size(0),
	collectCount(0), strings(32), staticRefs(nullptr),
	mainHeap(nullptr), largeObjectHeap(nullptr),
	gen0Base(nullptr), gen0Current(nullptr), gen0End(nullptr),
	gcoLists(nullptr),
	allocSection(5000),
	vm(owner)
{ }

GC::~GC()
{
	// Clean up all objects
	GCObject *gco = collectList;
	while (gco)
	{
		GCObject *next = gco->next;
		Release(gco);
		gco = next;
	}
	collectList = nullptr;

	gco = pinnedList;
	while (gco)
	{
		GCObject *next = gco->next;
		Release(gco);
		gco = next;
	}
	pinnedList = nullptr;

	// And delete static reference blocks, too
	StaticRefBlock *refs = staticRefs;
	while (refs)
	{
		StaticRefBlock *next = refs->next;
		delete refs;
		refs = next;
	}
	staticRefs = nullptr;

	DestroyHeaps();
}

bool GC::InitializeHeaps()
{
	// Create the mainHeap with enough initial memory for the gen0 chunk
	if (!os::HeapCreate(&mainHeap, GEN0_SIZE))
		return false;

	// The LOH has no initial size
	if (!os::HeapCreate(&largeObjectHeap, 0))
		return false;

	// Allocate gen0
	gen0Base = os::HeapAlloc(&mainHeap, GEN0_SIZE, false);
	if (!gen0Base)
		// This shouldn't happen since mainHeap is initialized with
		// a size that should be enough for gen0, but let's check
		// for it anyway.
		return false;
	gen0End = (char*)gen0Base + GEN0_SIZE;
	gen0Current = (char*)gen0Base;

	return true;
}

void GC::DestroyHeaps()
{
	if (mainHeap)
		os::HeapDestroy(&mainHeap);
	if (largeObjectHeap)
		os::HeapDestroy(&largeObjectHeap);
}

GCObject *GC::AllocRaw(size_t size)
{
	using namespace std;
	assert(size >= GCO_SIZE);

	GCObject *result;
	if (size > LARGE_OBJECT_SIZE)
	{
		result = (GCObject*)os::HeapAlloc(&largeObjectHeap, size, true);
		if (result)
			result->flags |= GCOFlags::LARGE_OBJECT;
	}
	else
	{
		// If there were any pinned objects in the last GC cycle,
		// we must verify that the new GCObject doesn't overlap
		// any pinned object. If so, we position the gen0Current
		// pointer behind the last pinned object where space is
		// available.
		if (pinnedList)
		{
			GCObject *pinned = pinnedList;
			// Given the ranges [a, b) and [c, d), e.g.:
			//      a         b
			//      [---------)
			//   [-------)
			//   c       d
			// the ranges are overlap if c < b and a < d.
			// In our case,
			//    a = pinned
			//    b = (char*)pinned + pinned->size
			//    c = gen0Current
			//    d = gen0Current + size
			while (pinned &&
				gen0Current < (char*)pinned + pinned->size &&
				(char*)pinned < gen0Current + size)
			{
				GCObject *next = pinned->next;
				// The pinned list is only traversed in one direction;
				// it is not necessary to call RemoveFromList first.
				// InsertIntoList updates the prev and next pointers
				// on the GCObject.
				pinned->InsertIntoList(&collectList);
				gen0Current = (char*)pinned + OVUM_ALIGN_TO(pinned->size, 8);

				pinned = next;
			}
			pinnedList = pinned;
		}

		result = (GCObject*)gen0Current;
		gen0Current += OVUM_ALIGN_TO(size, 8);
		if (gen0Current > gen0End)
		{
			// Not enough space in gen0. Return null, which forces a cycle.
			result = nullptr;
		}
		else
		{
			// Always zero all the memory before returning
			memset(result, 0, size);
			result->flags |= GCOFlags::GEN_0;
		}
	}

	return result;
}

GCObject *GC::AllocRawGen1(size_t size)
{
	// Don't call with HEAP_ZERO_MEMORY. We'll be copying the old
	// object into this address anyway, it'd be unnecessary work.
	return (GCObject*)os::HeapAlloc(&mainHeap, size, false);
}

void GC::ReleaseRaw(GCObject *gco)
{
	switch (gco->flags & GCOFlags::GENERATION)
	{
	// Do nothing with gen0 objects
	case GCOFlags::GEN_1:
		gen1Size -= gco->size;
		os::HeapFree(&mainHeap, gco);
		break;
	case GCOFlags::LARGE_OBJECT:
		os::HeapFree(&largeObjectHeap, gco);
		break;
	}
}

int GC::Alloc(Thread *const thread, Type *type, size_t size, GCObject **output)
{
	if (SIZE_MAX - size < GCO_SIZE)
		return thread->ThrowMemoryError(gc_strings::ObjectTooBig);

	BeginAlloc(thread);

	size += GCO_SIZE;
	GCObject *gco = AllocRaw(size);

	if (!gco) // Allocation failed (we're probably out of memory)
	{
		RunCycle(thread, size >= LARGE_OBJECT_SIZE);  // Try to free some memory...
		// Note: call RunCycle instead of Collect, because Collect calls
		// BeginAlloc to protect instance members. We've already called
		// that method, so we don't need to do it again.

		gco = AllocRaw(size); // ... And allocate again

		if (!gco)
			return OVUM_ERROR_NO_MEMORY;
	}

	// AllocRaw zeroes the memory, so DO NOT do that here.
	gco->size = size;
	gco->type = type;
	gco->flags |= currentCollectMark;
	gco->InsertIntoList(&collectList);

	*output = gco;

	EndAlloc();

	RETURN_SUCCESS;
}

int GC::Alloc(Thread *const thread, Type *type, size_t size, Value *output)
{
	GCObject *gco;
	int r = Alloc(thread, type, size, &gco);
	if (r == OVUM_SUCCESS)
	{
		output->type = type;
		output->v.instance = gco->InstanceBase();
	}
	return r;
}

int GC::AllocArray(Thread *const thread, uint32_t length, size_t itemSize, void **output)
{
	if (itemSize > 0 && length > SIZE_MAX / itemSize)
		return thread->ThrowOverflowError();

	GCObject *gco;
	int r = Alloc(thread, nullptr, length * itemSize, &gco);
	if (r != OVUM_SUCCESS) return r;

	gco->flags |= GCOFlags::ARRAY;
	*output = gco->InstanceBase();

	RETURN_SUCCESS;
}

int GC::AllocValueArray(Thread *const thread, uint32_t length, Value **output)
{
	if (length > SIZE_MAX / sizeof(Value))
		return thread->ThrowOverflowError();

	GCObject *gco;
	int r = Alloc(thread, (Type*)GC_VALUE_ARRAY, length * sizeof(Value), &gco);
	if (r != OVUM_SUCCESS) return r;

	gco->flags |= GCOFlags::ARRAY;
	*output = gco->FieldsBase();

	RETURN_SUCCESS;
}

void GC::BeginAlloc(Thread *const thread)
{
	if (allocSection.TryEnter() != OVUM_SUCCESS)
	{
		thread->EnterUnmanagedRegion();
		allocSection.Enter();
		thread->LeaveUnmanagedRegion();
	}
}

void GC::EndAlloc()
{
	allocSection.Leave();
}

int GC::Construct(Thread *const thread, Type *type, const uint16_t argc, Value *output)
{
	if (type == vm->types.String ||
		type->IsPrimitive() ||
		(type->flags & TypeFlags::ABSTRACT) == TypeFlags::ABSTRACT)
		return thread->ThrowTypeError();

	int r;
	StackFrame *frame = thread->currentFrame;
	Value *args = frame->evalStack + frame->stackCount - argc;
	if (output)
		r = ConstructLL(thread, type, argc, args, output);
	else
	{
		r = ConstructLL(thread, type, argc, args, args);
		if (r == OVUM_SUCCESS)
			frame->stackCount++;
	}
	return r;
}

int GC::ConstructLL(Thread *const thread, Type *type, const uint16_t argc, Value *args, Value *output)
{
	GCObject *gco;
	int r = Alloc(thread, type, type->GetTotalSize(), &gco);
	if (r != OVUM_SUCCESS) return r;

	Value *framePointer = args + argc;

	// Unshift value onto beginning of eval stack! Hurrah.
	for (int i = 0; i < argc; i++)
	{
		*framePointer = *(framePointer - 1);
		framePointer--;
	}

	framePointer->type = type;
	framePointer->v.instance = gco->InstanceBase();
	thread->currentFrame->stackCount++;

	Value ignore; // all Ovum methods return values, even the constructor
	r = thread->InvokeMethodOverload(type->instanceCtor->ResolveOverload(argc), argc, framePointer, &ignore);

	if (r == OVUM_SUCCESS)
	{
		output->type = type;
		output->v.instance = gco->InstanceBase();
	}

	return r;
}

String *GC::ConstructString(Thread *const thread, const int32_t length, const uchar value[])
{
	GCObject *gco;
	// Note: sizeof(String) includes firstChar, but we need an extra character
	// for the terminating \0 anyway. So this is fine.
	int r = Alloc(thread, vm->types.String, sizeof(String) + length*sizeof(uchar), &gco);
	if (r != OVUM_SUCCESS) return nullptr;

	MutableString *str = reinterpret_cast<MutableString*>(gco->InstanceBase());
	str->length = length;
	// Note: Alloc() initializes the bytes to 0. The default values of
	// hashCode and flags are both 0, so we don't need to set either here.

	// If you pass a null value, you get a string with nothing but \0s.
	if (value != nullptr)
		// Note: this does NOT include the terminating \0, which is fine.
		CopyMemoryT(&str->firstChar, value, length);

	return reinterpret_cast<String*>(str);
}

String *GC::ConvertString(Thread *const thread, const char *string)
{
	size_t length = strlen(string);

	if (length > INT32_MAX)
		return nullptr;

	String *output = ConstructString(thread, (int32_t)length, nullptr);

	if (output && length > 0)
	{
		uchar *mutch = const_cast<uchar*>(&output->firstChar);
		while (*string)
			*mutch++ = *string++;
	}

	return output;
}

String *GC::ConstructModuleString(Thread *const thread, const int32_t length, const uchar value[])
{
	// Replicate some functionality of Alloc here
	size_t size = sizeof(String) + length*sizeof(uchar) + GCO_SIZE;

	GCObject *gco = AllocRawGen1(size);
	if (!gco)
		throw ModuleLoadException(L"(none)", "Not enough memory for module string.");

	// AllocRawGen1 does NOT zero the memory, so we have to do that ourselves:
	memset(gco, 0, size);

	// Pin the strings so that they will never move, even if we later update
	// the GC to compact gen1
	gco->size = size;
	gco->type = vm->types.String;
	gco->flags |= currentCollectMark | GCOFlags::PINNED;
	if (gco->type == nullptr)
		gco->flags |= GCOFlags::EARLY_STRING;
	gco->pinCount++;
	gco->InsertIntoList(&collectList);

	MutableString *str = reinterpret_cast<MutableString*>(gco->InstanceBase());
	str->length = length;
	CopyMemoryT(&str->firstChar, value, length);

	return reinterpret_cast<String*>(str);
}

String *GC::GetInternedString(Thread *const thread, String *value)
{
	BeginAlloc(thread);
	String *result = strings.GetInterned(value);
	EndAlloc();
	return result;
}

bool GC::HasInternedString(Thread *const thread, String *value)
{
	BeginAlloc(thread);
	bool result = strings.HasInterned(value);
	EndAlloc();
	return result;
}

String *GC::InternString(Thread *const thread, String *value)
{
	BeginAlloc(thread);
	String *result = strings.Intern(value);
	EndAlloc();
	return result;
}

void GC::Release(GCObject *gco)
{
	assert((gco->flags & GCOFlags::MARK) == currentCollectMark);

	if (gco->IsEarlyString() || gco->type == vm->types.String)	
	{
		String *str = reinterpret_cast<String*>(gco->InstanceBase());
		if ((str->flags & StringFlags::INTERN) != StringFlags::NONE)
			strings.RemoveIntern(str);
	}
	else if (!gco->IsArray() && gco->type->HasFinalizer())
	{
		Type *type = gco->type;
		do
		{
			if (type->finalizer)
				type->finalizer(gco->InstanceBase(type));
		} while (type = type->baseType);
	}

	ReleaseRaw(gco); // goodbye, dear pointer.
}

void GC::AddMemoryPressure(Thread *const thread, const size_t size)
{
	// Not implemented yet
}

void GC::RemoveMemoryPressure(Thread *const thread, const size_t size)
{
	// Not implemented yet
}

StaticRef *GC::AddStaticReference(Thread *const thread, Value value)
{
	BeginAlloc(thread);

	StaticRef *output;
	if (staticRefs == nullptr ||
		staticRefs->count == StaticRefBlock::BLOCK_SIZE)
	{
		StaticRefBlock *newBlock = new(std::nothrow) StaticRefBlock(staticRefs);
		if (!newBlock)
		{
			output = nullptr; // No moar memory
			goto done;
		}
		staticRefs = newBlock;
	}

	output = staticRefs->values + staticRefs->count++;
	output->Init(value);

done:
	EndAlloc();
	return output;
}

void GC::Collect(Thread *const thread, bool collectGen1)
{
	// Make sure nothing else touches the instance during the cycle
	BeginAlloc(thread);

	RunCycle(thread, collectGen1);

	EndAlloc();
}

void GC::RunCycle(Thread *const thread, bool collectGen1)
{
	BeginCycle(thread);

	collectCount++;

	// Upon entering this method, all objects are in collectList and pinnedList.
	// The pinned list is usually empty when we enter here, but a cycle can be
	// triggered when the pinned objects take up too much space or leave gaps
	// too small to fit an object into, or when a large object can't be allocated.
	//
	// Let's start by copying all pinned objects into the Collect list. During
	// the cycle, we'll rebuild the pinned list anyway.
	if (pinnedList)
	{
		GCObject *pinned = pinnedList;
		while (pinned)
		{
			GCObject *next = pinned->next;
			// No need to call RemoveFromList first;
			// we're accessing the items sequentially,
			// and nothing else will touch the list.
			pinned->InsertIntoList(&collectList);
			pinned = next;
		}
		pinnedList = nullptr;
	}

	TempLists gcoLists = { }; // initialize everything to zero
	this->gcoLists = &gcoLists;
	// Step 1: Move all the root objects to the Process list.
	MarkRootSet();

	// Step 2: Examine all objects in the Process list.
	// Objects are grouped into one of the following:
	// * Gen0 survivors (including pinned objects)
	//     => gcoLists->survivors.gen0
	// * Survivors (from gen1 or LOH) with refs to non-pinned gen0 objects;
	//   that is, any non-gen0 object that needs updating later
	//     => gcoLists->survivors.withGen0Refs
	// * All other survivors
	//     => gcoLists->keep
	// During this step, we also update gcoLists->survivors.gen1SurvivorSize,
	// which will later help us determine whether gen1 has enough dead
	// objects to warrant cleaning it up this cycle.
	while (gcoLists.process)
	{
		GCObject *item = gcoLists.process;
		do
		{
			GCObject *next = item->next;
			ProcessObjectAndFields(item);
			item = next;
		} while (item);
	}
	assert(gcoLists.process == nullptr);

	// Step 3: Process gen0 survivors.
	// For each object:
	// * If the object is pinned, add it to the list of pinned objects.
	// * Otherwise, allocate gen1 space for the object, move the data, and
	//   mark the original gen0 location with GCOFlags::MOVED.
	// * Then, if the object has gen0 refs, add it to the list of such objects;
	//   otherwise, move it to the Keep list (nothing more to process).
	MoveGen0Survivors();
	assert(gcoLists.survivors.gen0 == nullptr);

	// Step 4: Update objects with gen0 references.
	// An astute reader may have noticed that pinned objects with gen0 refs
	// are not actually in gcoLists->survivors.withGen0Refs, but in pinnedList.
	// For this reason, we walk through those here as well. The number of
	// pinned objects is likely to be small.
	// We must also not forget to update root references; unfortunately,
	// this means we have to walk through the entire root set.
	UpdateGen0References();
	assert(gcoLists.survivors.withGen0Refs == nullptr);

	// Step 5: Collect garbage.
	// Finalize any collectible dead objects with finalizers, and release the
	// memory. We only collect gen1 if collectGen1 is true, or if there are
	// enough dead objects in it.
	if (!collectGen1)
		collectGen1 = gen1Size - gcoLists.survivors.gen1SurvivorSize >= GEN1_DEAD_OBJECTS_THRESHOLD;

	{
		GCObject *item = collectList;
		while (item)
		{
			GCObject *next = item->next;

			if (collectGen1 || (item->flags & GCOFlags::GENERATION) != GCOFlags::GEN_1)
				Release(item);
			else
			{
				// Uncollectible gen1 object, will be collected in the future
				item->InsertIntoList(&gcoLists.keep);
				// Make sure it's marked GCO_COLLECT next cycle
				item->Mark(currentKeepMark);
			}

			item = next;
		}
		collectList = nullptr;
	}

	// The Keep and Pinned lists should contain all the live objects now,
	// and all other lists should be empty.
	assert(gcoLists.survivors.gen0         == nullptr);
	assert(gcoLists.survivors.withGen0Refs == nullptr);
	assert(gcoLists.process                == nullptr);
	assert(collectList                     == nullptr);

	// Step 6: Swap currentCollectMark and currentKeepMark for
	// next cycle and set current Keep list to Collect.
	std::swap(currentCollectMark, currentKeepMark);
	collectList = gcoLists.keep;
	gen0Current = (char*)gen0Base;
	// And reset this to null
	this->gcoLists = nullptr;

	EndCycle(thread);
}

void GC::BeginCycle(Thread *const thread)
{
	// Future change: suspend every thread except the current
}

void GC::EndCycle(Thread *const thread)
{
	// Future change: resume every thread except the current
}

void GC::MarkRootSet()
{
	Thread *const mainThread = vm->mainThread;

	bool hasGen0Refs;
	// Mark stack frames first.
	StackFrame *frame = mainThread->currentFrame;
	// Frames are marked top-to-bottom.
	while (frame && frame->method)
	{
		MethodOverload *method = frame->method;
		// Does the method have any parameters?
		unsigned int paramCount = method->GetEffectiveParamCount();
		if (paramCount)
			ProcessLocalValues(paramCount, (Value*)frame - paramCount);
		// By design, the locals and the eval stack are adjacent in memory.
		// Hence, the following is safe:
		if (method->locals || frame->stackCount)
			ProcessLocalValues(method->locals + frame->stackCount, frame->Locals());

		frame = frame->prevFrame;
	}

	// We need to do this because the GC may be triggered in
	// a finally clause, and we wouldn't want to obliterate
	// the error if we still need to catch it later, right?
	TryMarkForProcessing(&mainThread->currentError, &hasGen0Refs);

	// Examine module strings! We don't want to collect these, even
	// if there is nothing referencing them anywhere else.
	ModulePool *loadedModules = vm->GetModulePool();
	for (int i = 0; i < loadedModules->GetLength(); i++)
	{
#ifndef NDEBUG
		hasGen0Refs = false;
#endif

		Module *m = loadedModules->Get(i);
		TryMarkStringForProcessing(m->name, &hasGen0Refs);

		for (int32_t s = 0; s < m->strings.GetLength(); s++)
			TryMarkStringForProcessing(m->strings[s], &hasGen0Refs);

		if (m->debugData)
		{
			debug::ModuleDebugData *debug = m->debugData;
			for (int32_t f = 0; f < debug->fileCount; f++)
				TryMarkStringForProcessing(debug->files[f].fileName, &hasGen0Refs);
		}

		// Module strings are supposed to be all in gen1
		assert(!hasGen0Refs);
	}

	// And then we have all the beautiful, lovely static references.
	StaticRefBlock *staticRefs = this->staticRefs;
	while (staticRefs)
	{
		for (unsigned int i = 0; i < staticRefs->count; i++)
			TryMarkForProcessing(&staticRefs->values[i].value, &staticRefs->hasGen0Refs);
		staticRefs = staticRefs->next;
	}
}

bool GC::ShouldProcess(Value *val, bool *hasGen0Refs)
{
	if (val->type == nullptr || val->type->IsPrimitive())
		return false;

	if (val->type == vm->types.String &&
		(val->v.string->flags & StringFlags::STATIC) == StringFlags::STATIC)
		return false;

	// If gco is a non-pinned gen0 object, set *hasGen0Refs to true.
	GCOFlags flags = GCObject::FromValue(val)->flags;
	if ((flags & GCOFlags::GEN_0) == GCOFlags::GEN_0 &&
		(flags & GCOFlags::PINNED) == GCOFlags::NONE)
		*hasGen0Refs = true;

	return (flags & GCOFlags::MARK) == currentCollectMark;
}

void GC::TryMarkForProcessing(Value *value, bool *hasGen0Refs)
{
	if (ShouldProcess(value, hasGen0Refs))
		MarkForProcessing(GCObject::FromValue(value));
}

void GC::TryMarkStringForProcessing(String *str, bool *hasGen0Refs)
{
	if (str && (str->flags & StringFlags::STATIC) == StringFlags::NONE)
	{
		GCObject *gco = GCObject::FromInst(str);
		if ((gco->flags & GCOFlags::GEN_0) == GCOFlags::GEN_0)
			*hasGen0Refs = true;
		if ((gco->flags & GCOFlags::MARK) == currentCollectMark)
			MarkForProcessing(gco);
	}
}

void GC::ProcessFields(unsigned int fieldCount, Value fields[], bool *hasGen0Refs)
{
	for (unsigned int i = 0; i < fieldCount; i++)
		// If the object is marked GCO_KEEP, we're done processing it.
		// If it's marked GCO_PROCESS, it'll be processed eventually.
		// Otherwise, mark it for processing!
		TryMarkForProcessing(fields + i, hasGen0Refs);
}

void GC::ProcessLocalValues(unsigned int count, Value values[])
{
	bool _;
	for (unsigned int i = 0; i < count; i++)
	{
		Value *v = values + i;
		if ((uintptr_t)v->type & 1)
		{
			// There is no need to examine local or static field references;
			// we'll get to the underlying storage locations when we examine
			// the root set. Instance field references do need to be looked
			// at, because the instance is still alive and we may have the
			// only remaining reference to it.
			if ((uintptr_t)v->type != LOCAL_REFERENCE &&
				(uintptr_t)v->type != STATIC_REFERENCE)
			{
				GCObject *gco = reinterpret_cast<GCObject*>((char*)v->v.reference - ~(uintptr_t)v->type);
				if ((gco->flags & GCOFlags::MARK) == currentCollectMark)
					MarkForProcessing(gco);
			}
		}
		else
			TryMarkForProcessing(v, &_);
	}
}

void GC::MarkForProcessing(GCObject *gco)
{
	// Must move from collect to process.
	assert((gco->flags & GCOFlags::MARK) == currentCollectMark);

	gco->RemoveFromList(gco == pinnedList ? &pinnedList : &collectList);
	// If gco->type is null, then the gco must be an EARLY_STRING or an ARRAY;
	// in both cases, we couldn't possibly have any instance fields. If the
	// type is GC_VALUE_ARRAY or has a size greater than zero, we might find
	// some fields after all.
	assert(gco->IsEarlyString() ? gco->type == nullptr :
		gco->IsArray() ? gco->type == nullptr || gco->type == (Type*)GC_VALUE_ARRAY :
		gco->type != nullptr);
	bool couldHaveFields = gco->type != nullptr &&
		(gco->type == (Type*)GC_VALUE_ARRAY || gco->type->GetTotalSize() > 0);

	if (couldHaveFields)
	{
		gco->InsertIntoList(&gcoLists->process);
		gco->Mark(GCOFlags::PROCESS);
	}
	else
	{
		// No chance of instance fields, so nothing to process
		AddSurvivor(gco);
		gco->Mark(currentKeepMark);
	}
}

void GC::AddSurvivor(GCObject *gco)
{
	GCObject **list;
	if ((gco->flags & GCOFlags::GENERATION) == GCOFlags::GEN_0)
		list = &gcoLists->survivors.gen0;
	else
	{
		if (gco->HasGen0Refs())
			list = &gcoLists->survivors.withGen0Refs;
		else
			list = &gcoLists->keep;
		if ((gco->flags & GCOFlags::GENERATION) == GCOFlags::GEN_1)
			gcoLists->survivors.gen1SurvivorSize += gco->size;
	}
	gco->InsertIntoList(list);
}

void GC::ProcessObjectAndFields(GCObject *gco)
{
	// The object is not supposed to be anything but GCOFlags::PROCESS at this point.
	assert((gco->flags & GCOFlags::MARK) == GCOFlags::PROCESS);
	// It's also not supposed to be a value type, but could be a GC value array.
	assert(!gco->type || gco->type == (Type*)GC_VALUE_ARRAY || !gco->type->IsPrimitive());

	// Do this first, so that objects referencing this object will not
	// attempt to re-mark it for processing
	gco->Mark(currentKeepMark);

	bool hasGen0Refs = false;
	Type *type = gco->type;
	if (type == (Type*)GC_VALUE_ARRAY)
	{
		ProcessFields((gco->size - GCO_SIZE) / sizeof(Value), gco->FieldsBase(), &hasGen0Refs);
	}
	else
	{
		while (type)
		{
			if ((type->flags & TypeFlags::CUSTOMPTR) != TypeFlags::NONE)
				ProcessCustomFields(type, gco->InstanceBase(), &hasGen0Refs);
			else if (type->fieldCount)
				ProcessFields(type->fieldCount, gco->FieldsBase(type), &hasGen0Refs);

			type = type->baseType;
		}
	}

	if (hasGen0Refs)
		gco->flags |= GCOFlags::HAS_GEN0_REFS;

	gco->RemoveFromList(&gcoLists->process);
	// Insert into the appropriate survivor list
	AddSurvivor(gco);
}

void GC::ProcessCustomFields(Type *type, void *instBase, bool *hasGen0Refs)
{
	// Process native fields first
	for (int i = 0; i < type->fieldCount; i++)
	{
		Type::NativeField field = type->nativeFields[i];
		void *fieldPtr = (char*)instBase + field.offset;
		switch (field.type)
		{
		case NativeFieldType::VALUE:
			TryMarkForProcessing(reinterpret_cast<Value*>(fieldPtr), hasGen0Refs);
			break;
		case NativeFieldType::VALUE_PTR:
			TryMarkForProcessing(*reinterpret_cast<Value**>(fieldPtr), hasGen0Refs);
			break;
		case NativeFieldType::STRING:
			TryMarkStringForProcessing(*reinterpret_cast<String**>(fieldPtr), hasGen0Refs);
			break;
		case NativeFieldType::GC_ARRAY:
			if (*(void**)fieldPtr)
			{
				GCObject *gco = GCObject::FromInst(*(void**)fieldPtr);
				GCOFlags flags = gco->flags;
				if ((flags & GCOFlags::GEN_0) == GCOFlags::GEN_0 &&
					(flags & GCOFlags::PINNED) == GCOFlags::NONE)
					*hasGen0Refs = true;
				if ((flags & GCOFlags::MARK) == currentCollectMark)
					MarkForProcessing(gco);
			}
			break;
		}
	}

	// If the type has no reference getter, assume it has no managed references
	if (type->getReferences)
	{
		FieldProcessState state;
		state.gc = this;
		state.hasGen0Refs = hasGen0Refs;
		type->getReferences((char*)instBase + type->fieldsOffset,
			ProcessFieldsCallback, &state);
	}
}

int GC::ProcessFieldsCallback(void *state, unsigned int count, Value *values)
{
	FieldProcessState *fps = reinterpret_cast<FieldProcessState*>(state);
	fps->gc->ProcessFields(count, values, fps->hasGen0Refs);
	RETURN_SUCCESS;
}

void GC::MoveGen0Survivors()
{
	GCObject **list = &gcoLists->survivors.gen0;
	size_t *gen1SurvivorSize = &gcoLists->survivors.gen1SurvivorSize;

	GCObject *obj = *list;
	while (obj)
	{
		GCObject *next = obj->next;

		obj->RemoveFromList(list);
		if (!obj->IsPinned())
		{
			// If the object is not pinned, then allocate gen1 space for it.
			GCObject *newAddress = AllocRawGen1(obj->size);
			if (newAddress == nullptr)
				// Not enough available memory to move to generation 1;
				// cannot recover from this.
				abort();

			memcpy(newAddress, obj, obj->size);
			newAddress->flags = (newAddress->flags & ~GCOFlags::GENERATION) | GCOFlags::GEN_1;
			newAddress->InsertIntoList(newAddress->HasGen0Refs() ? &gcoLists->survivors.withGen0Refs : &gcoLists->keep);
			gen1Size += newAddress->size;
			*gen1SurvivorSize += newAddress->size;

			obj->flags |= GCOFlags::MOVED;
			obj->newAddress = newAddress;

			if (newAddress->type == vm->types.String ||
				(newAddress->flags & GCOFlags::EARLY_STRING) == GCOFlags::EARLY_STRING)
			{
				String *str = reinterpret_cast<String*>(newAddress->InstanceBase());
				if ((str->flags & StringFlags::INTERN) == StringFlags::INTERN)
					strings.UpdateIntern(str);
			}
		}
		else
		{
			AddPinnedObject(obj);
		}

		obj = next;
	}

	if (pinnedList)
	{
		GCObject *lastPinned; // ignored
		pinnedList = FlattenPinnedTree(pinnedList, &lastPinned);
	}
}

void GC::AddPinnedObject(GCObject *gco)
{
	// We initially store the pinned objects in a binary search tree,
	// which is then flattened to a linked list when we're done with
	// moving gen0 survivors. Depending on the order in which we walk
	// through pinned objects, this tree may be terribly unbalanced,
	// but the number of pinned objects should be small and therefore
	// the performance impact negligible.
	// 'prev' is used as the left node (numerically less than the GCO),
	// 'next' as the right (numerically greater than the GCO).
	gco->prev = gco->next = nullptr;

	GCObject **root = &pinnedList;
	while (true)
	{
		if (!*root)
		{
			*root = gco;
			break;
		}
		else if (gco < *root)
			root = &(*root)->prev;
		else if (gco > *root)
			root = &(*root)->next;
#if !defined(NDEBUG) || NDEBUG == 0
		else
		{
			assert(!"Failed to insert pinned object into tree; it's probably in the tree already!");
			break; // fail :(
		}
#endif
	}
}

GCObject *GC::FlattenPinnedTree(GCObject *root, GCObject **lastItem)
{
	GCObject *first = root;
	*lastItem = root;
	if (root->prev)
	{
		GCObject *leftLast;
		first = FlattenPinnedTree(root->prev, &leftLast);
		leftLast->next = root;
	}
	if (root->next)
	{
		root->next = FlattenPinnedTree(root->next, lastItem);
	}
	return first;
}

void GC::UpdateGen0References()
{
	UpdateRootSet();

	GCObject *gco = gcoLists->survivors.withGen0Refs;
	while (gco)
	{
		GCObject *next = gco->next;

		gco->RemoveFromList(&gcoLists->survivors.withGen0Refs);
		gco->InsertIntoList(&gcoLists->keep);
		UpdateObjectFields(gco);

		gco = next;
	}

	gco = pinnedList;
	while (gco)
	{
		if (gco->HasGen0Refs())
			UpdateObjectFields(gco);
		gco = gco->next;
	}
}

void GC::UpdateRootSet()
{
	Thread *const mainThread = vm->mainThread;

	// Update stack frames first
	StackFrame *frame = mainThread->currentFrame;
	while (frame && frame->method)
	{
		MethodOverload *method = frame->method;
		unsigned int paramCount = method->GetEffectiveParamCount();
		if (paramCount)
			UpdateLocals(paramCount, (Value*)frame - paramCount);

		if (method->locals || frame->stackCount)
			UpdateLocals(method->locals + frame->stackCount, frame->Locals());

		frame = frame->prevFrame;
	}

	// Current error
	TryUpdateRef(&mainThread->currentError);

	// Module strings do not have any gen0 references

	// Static references
	StaticRefBlock *staticRefs = this->staticRefs;
	while (staticRefs)
	{
		if (staticRefs->hasGen0Refs)
		{
			for (unsigned int i = 0; i < staticRefs->count; i++)
				TryUpdateRef(&staticRefs->values[i].value);
			staticRefs->hasGen0Refs = false;
		}
		staticRefs = staticRefs->next;
	}
}

void GC::UpdateObjectFields(GCObject *gco)
{
	Type *type = gco->type;
	if (type == (Type*)GC_VALUE_ARRAY)
	{
		UpdateFields((gco->size - GCO_SIZE) / sizeof(Value), gco->FieldsBase());
	}
	else
	{
		while (type)
		{
			if ((type->flags & TypeFlags::CUSTOMPTR) != TypeFlags::NONE)
				UpdateCustomFields(type, gco->InstanceBase());
			else if (type->fieldCount)
				UpdateFields(type->fieldCount, gco->FieldsBase(type));

			type = type->baseType;
		}
	}

	gco->flags &= ~GCOFlags::HAS_GEN0_REFS;
}

void GC::UpdateCustomFields(Type *type, void *instBase)
{
	// Update native fields first
	for (int i = 0; i < type->fieldCount; i++)
	{
		Type::NativeField field = type->nativeFields[i];
		void *fieldPtr = (char*)instBase + field.offset;
		switch (field.type)
		{
		case NativeFieldType::VALUE:
			TryUpdateRef(reinterpret_cast<Value*>(fieldPtr));
			break;
		case NativeFieldType::VALUE_PTR:
			TryUpdateRef(*reinterpret_cast<Value**>(fieldPtr));
			break;
		case NativeFieldType::STRING:
			TryUpdateStringRef(reinterpret_cast<String**>(fieldPtr));
			break;
		case NativeFieldType::GC_ARRAY:
			if (*(void**)fieldPtr)
			{
				GCObject *gco = GCObject::FromInst(*(void**)fieldPtr);
				if (gco->IsMoved())
					*(void**)fieldPtr = gco->newAddress->InstanceBase();
			}
			break;
		}
	}

	// If the type has no reference getter, assume it has no managed references
	if (type->getReferences)
		type->getReferences((char*)instBase + type->fieldsOffset,
			UpdateFieldsCallback, this);
}

int GC::UpdateFieldsCallback(void *state, unsigned int count, Value *values)
{
	reinterpret_cast<GC*>(state)->UpdateFields(count, values);
	RETURN_SUCCESS;
}

bool GC::ShouldUpdateRef(Value *val)
{
	if (val->type == nullptr || val->type->IsPrimitive())
		return false;

	if (val->type == vm->types.String &&
		(val->v.string->flags & StringFlags::STATIC) == StringFlags::STATIC)
		return false;

	return GCObject::FromValue(val)->IsMoved();
}

void GC::TryUpdateRef(Value *value)
{
	if (ShouldUpdateRef(value))
		value->v.instance = GCObject::FromValue(value)->newAddress->InstanceBase();
}

void GC::TryUpdateStringRef(String **str)
{
	if (*str && ((*str)->flags & StringFlags::STATIC) == StringFlags::NONE)
	{
		GCObject *gco = GCObject::FromInst(*str);
		if (gco->IsMoved())
			*str = (String*)gco->newAddress->InstanceBase();
	}
}

void GC::UpdateFields(unsigned int fieldCount, Value fields[])
{
	for (unsigned int i = 0; i < fieldCount; i++)
		TryUpdateRef(fields + i);
}

void GC::UpdateLocals(unsigned int count, Value values[])
{
	for (unsigned int i = 0; i < count; i++)
	{
		Value *v = values + i;
		if ((uintptr_t)v->type & 1)
		{
			// Local and static refs are immovable, but field refs are not.
			if ((uintptr_t)v->type != LOCAL_REFERENCE &&
				(uintptr_t)v->type != STATIC_REFERENCE)
			{
				uintptr_t offset = ~(uintptr_t)v->type;
				GCObject *gco = reinterpret_cast<GCObject*>((char*)v->v.reference - offset);
				if (gco->IsMoved())
					v->v.reference = (char*)gco->newAddress + offset;
			}
		}
		else
			TryUpdateRef(v);
	}
}

} // namespace ovum

OVUM_API int GC_Construct(ThreadHandle thread, TypeHandle type, const uint16_t argc, Value *output)
{
	return thread->GetGC()->Construct(thread, type, argc, output);
}

OVUM_API String *GC_ConstructString(ThreadHandle thread, const int32_t length, const uchar *values)
{
	return thread->GetGC()->ConstructString(thread, length, values);
}

OVUM_API int GC_AllocArray(ThreadHandle thread, uint32_t length, size_t itemSize, void **output)
{
	return thread->GetGC()->AllocArray(thread, length, itemSize, output);
}

OVUM_API int GC_AllocValueArray(ThreadHandle thread, uint32_t length, Value **output)
{
	return thread->GetGC()->AllocValueArray(thread, length, output);
}

OVUM_API void GC_AddMemoryPressure(ThreadHandle thread, const size_t size)
{
	thread->GetGC()->AddMemoryPressure(thread, size);
}

OVUM_API void GC_RemoveMemoryPressure(ThreadHandle thread, const size_t size)
{
	thread->GetGC()->RemoveMemoryPressure(thread, size);
}

OVUM_API Value *GC_AddStaticReference(ThreadHandle thread, Value initialValue)
{
	ovum::StaticRef *ref = thread->GetGC()->AddStaticReference(thread, initialValue);
	if (ref == nullptr)
		return nullptr;
	return ref->GetValuePointer();
}

OVUM_API void GC_Collect(ThreadHandle thread)
{
	thread->GetGC()->Collect(thread, false);
}

OVUM_API uint32_t GC_GetCollectCount(ThreadHandle thread)
{
	return thread->GetGC()->GetCollectCount();
}

OVUM_API int GC_GetGeneration(Value *value)
{
	using namespace ovum;

	if (value->type->IsPrimitive())
		return -1;

	GCObject *gco = GCObject::FromValue(value);
	switch (gco->flags & GCOFlags::GENERATION)
	{
	case GCOFlags::GEN_0:
		return 0;
	case GCOFlags::GEN_1:
	case GCOFlags::LARGE_OBJECT:
		return 1;
	default:
		return -1;
	}
}

OVUM_API uint32_t GC_GetObjectHashCode(Value *value)
{
	if (value->type == nullptr || value->type->IsPrimitive())
		return 0; // Nope!

	ovum::GCObject *gco = ovum::GCObject::FromValue(value);
	if (gco->hashCode == 0)
	{
		// Shift down by 3 because addresses are (generally) aligned on the 8-byte boundary
		if (sizeof(void*) == 8)
		{
			uint64_t addr = (uint64_t)gco >> 3;
			gco->hashCode = (uint32_t)addr ^ (uint32_t)(addr >> 23);
		}
		else
			gco->hashCode = (uint32_t)gco >> 3;
	}
	return gco->hashCode;
}

OVUM_API void GC_Pin(Value *value)
{
	using namespace std;
	if (value->type != nullptr && !value->type->IsPrimitive())
	{
		ovum::GCObject *gco = ovum::GCObject::FromValue(value);
		// We must synchronise access to these two fields.
		// Let's just reuse the field access lock.
		gco->fieldAccessLock.Enter();
		gco->pinCount++;
		gco->flags |= ovum::GCOFlags::PINNED;
		gco->fieldAccessLock.Leave();
	}
}

OVUM_API void GC_PinInst(void *value)
{
	using namespace std;
	if (value)
	{
		ovum::GCObject *gco = ovum::GCObject::FromInst(value);
		// We must synchronise access to these two fields.
		// Let's just reuse the field access lock.
		gco->fieldAccessLock.Enter();
		gco->pinCount++;
		gco->flags |= ovum::GCOFlags::PINNED;
		gco->fieldAccessLock.Leave();
	}
}

OVUM_API void GC_Unpin(Value *value)
{
	using namespace std;
	if (value->type != nullptr && !value->type->IsPrimitive())
	{
		ovum::GCObject *gco = ovum::GCObject::FromValue(value);
		// We must synchronise access to these two fields.
		// Let's just reuse the field access lock.
		gco->fieldAccessLock.Enter();
		gco->pinCount--;
		if (gco->pinCount == 0)
			gco->flags &= ~ovum::GCOFlags::PINNED;
		gco->fieldAccessLock.Leave();
	}
}

OVUM_API void GC_UnpinInst(void *value)
{
	using namespace std;
	if (value)
	{
		ovum::GCObject *gco = ovum::GCObject::FromInst(value);
		// We must synchronise access to these two fields.
		// Let's just reuse the field access lock.
		gco->fieldAccessLock.Enter();
		gco->pinCount--;
		if (gco->pinCount == 0)
			gco->flags &= ~ovum::GCOFlags::PINNED;
		gco->fieldAccessLock.Leave();
	}
}