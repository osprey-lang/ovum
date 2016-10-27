#include "gc.h"
#include "gcobject.h"
#include "staticref.h"
#include "liveobjectfinder.h"
#include "movedobjectupdater.h"
#include "../ee/thread.h"
#include "../module/module.h"
#include "../module/modulepool.h"
#include "../object/type.h"
#include "../object/method.h"
#include "../object/value.h"
#include "../debug/debugsymbols.h"
#include "../res/staticstrings.h"
#include "../config/defaults.h"

namespace ovum
{

Box<GC> GC::New(VM *owner)
{
	Box<GC> result(new(std::nothrow) GC(owner));
	if (!result)
		return nullptr;
	if (!result->InitializeHeaps())
		return nullptr;

	return std::move(result);
}

GC::GC(VM *owner) :
	collectList(nullptr),
	pinnedList(nullptr),
	currentWhite((GCOFlags)1),
	currentBlack((GCOFlags)3),
	gen1Size(0),
	collectCount(0),
	strings(32),
	staticRefs(),
	mainHeap(nullptr),
	largeObjectHeap(nullptr),
	gen0Base(nullptr),
	gen0Current(nullptr),
	gen0End(nullptr),
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

	DestroyHeaps();
}

bool GC::InitializeHeaps()
{
	// Create the mainHeap with enough initial memory for the gen0 chunk
	if (!os::HeapCreate(&mainHeap, config::Defaults::GEN0_SIZE))
		return false;

	// The LOH has no initial size
	if (!os::HeapCreate(&largeObjectHeap, 0))
		return false;

	// Allocate gen0
	gen0Base = os::HeapAlloc(&mainHeap, config::Defaults::GEN0_SIZE, false);
	if (!gen0Base)
		// This shouldn't happen since mainHeap is initialized with
		// a size that should be enough for gen0, but let's check
		// for it anyway.
		return false;
	gen0End = (char*)gen0Base + config::Defaults::GEN0_SIZE;
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
	OVUM_ASSERT(size >= GCO_SIZE);

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
		return thread->ThrowMemoryError(vm->GetStrings()->error.ObjectTooLarge);

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
	gco->flags |= currentWhite;
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

int GC::Construct(Thread *const thread, Type *type, ovlocals_t argc, Value *output)
{
	if (type == vm->types.String || type->IsAbstract())
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

int GC::ConstructLL(Thread *const thread, Type *type, ovlocals_t argc, Value *args, Value *output)
{
	int r;
	// Reserve space for the instance on the evaluation stack.
	// After this, realArgs will point to the instance.
	Value *realArgs = args + argc;

	for (ovlocals_t i = 0; i < argc; i++)
	{
		*realArgs = *(realArgs - 1);
		realArgs--;
	}

	realArgs->type = nullptr; // Start out with null
	thread->currentFrame->stackCount++;

	MethodOverload *ctor = type->instanceCtor->ResolveOverload(argc);
	// If the constructor has been marked as an allocator, then it
	// performs all the allocation of the instance itself.
	if (type->ConstructorIsAllocator())
	{
		// Just call straight through to the constructor.
		// The return value of the constructor call becomes the result
		// of the call to the GC. Note that only native methods can
		// return values from constructors.
		r = thread->InvokeMethodOverload(ctor, argc, realArgs, output);
	}
	else
	{
		// Allocate the instance
		GCObject *gco;
		r = Alloc(thread, type, type->GetTotalSize(), &gco);
		if (r != OVUM_SUCCESS)
			goto done;
		// And put it in the reserved stack slot
		realArgs->type = type;
		realArgs->v.instance = gco->InstanceBase();

		Value ignore; // Even the constructor returns a value
		r = thread->InvokeMethodOverload(ctor, argc, realArgs, &ignore);
		if (r != OVUM_SUCCESS)
			goto done;

		// If everything went okay, copy the result to the right place.
		// At this point, we CANNOT rely on gco->InstanceBase(), because
		// the constructor may have triggered a GC cycle, which means that
		// gco will be pointing to the old location. But realArgs is on
		// the managed stack, so it is guaranteed to have been updated,
		// and so we use that.
		output->type = realArgs->type;
		output->v.instance = realArgs->v.instance;
	}

done:
	return r;
}

String *GC::ConstructString(Thread *const thread, int32_t length, const ovchar_t value[])
{
	GCObject *gco;
	// Note: sizeof(String) includes firstChar, but we need an extra character
	// for the terminating \0 anyway. So this is fine.
	int r = Alloc(thread, vm->types.String, sizeof(String) + length*sizeof(ovchar_t), &gco);
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
		ovchar_t *mutch = const_cast<ovchar_t*>(&output->firstChar);
		while (*string)
			*mutch++ = *string++;
	}

	return output;
}

String *GC::ConstructModuleString(Thread *const thread, int32_t length, const ovchar_t value[])
{
	// Replicate some functionality of Alloc here
	size_t size = sizeof(String) + length*sizeof(ovchar_t) + GCO_SIZE;

	GCObject *gco = AllocRawGen1(size);
	if (!gco)
		throw ModuleLoadException(L"(none)", "Not enough memory for module string.");

	// AllocRawGen1 does NOT zero the memory, so we have to do that ourselves:
	memset(gco, 0, size);

	// Pin the strings so that they will never move, even if we later update
	// the GC to compact gen1
	gco->size = size;
	gco->type = vm->types.String;
	gco->flags |= currentWhite | GCOFlags::PINNED;
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
	OVUM_ASSERT(gco->GetColor() == currentWhite);

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

void GC::AddMemoryPressure(Thread *const thread, size_t size)
{
	// Not implemented yet
}

void GC::RemoveMemoryPressure(Thread *const thread, size_t size)
{
	// Not implemented yet
}

StaticRef *GC::AddStaticReference(Thread *const thread, Value *value)
{
	BeginAlloc(thread);

	StaticRef *output = nullptr;
	if (!staticRefs || staticRefs->IsFull())
	{
		if (!StaticRefBlock::Extend(staticRefs))
			// No more memory. Return null to signal failure.
			goto done;
	}

	output = staticRefs->Add(value);

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

	// Step 1: Find all live objects.
	// During this step, we also separate survivors into one of three groups:
	//
	// * Survivors from generation 0.
	// * Survivors with references to generation 0 objects.
	// * All other survivors.
	//
	// See LiveObjectFinder for more details on each group.
	LiveObjectFinder liveFinder(this);
	liveFinder.FindLiveObjects();

	// Step 2: Process gen0 survivors.
	// For each object:
	// * If the object is pinned, add it to the list of pinned objects.
	// * Otherwise, allocate gen1 space for the object, move the data, and mark
	//   the original gen0 location with GCOFlags::MOVED.
	// * Then, if the object has gen0 refs, add it to the list of such objects;
	//   otherwise, move it to the "keep" list (nothing more to process).
	MoveGen0Survivors(liveFinder);
	OVUM_ASSERT(liveFinder.survivorsFromGen0 == nullptr);

	// Step 3: Update objects with gen0 references.
	// An astute reader may have noticed that pinned objects with gen0 refs are
	// not actually in liveFinder.survivorsWithGen0Refs, but in pinnedList. For
	// this reason, we walk through those here as well. The number of pinned
	// objects is likely to be small, so the performance impact negligible.
	UpdateGen0References(liveFinder);
	OVUM_ASSERT(liveFinder.survivorsWithGen0Refs == nullptr);

	// Step 4: Collect garbage.
	// Finalize any collectible dead objects with finalizers, and release the
	// memory. We only collect gen1 if collectGen1 is true, or if there are
	// enough dead objects in it.
	CollectGarbage(liveFinder, collectGen1);

	// The "keep" and "pinned" lists should contain all the live objects now,
	// and all other lists should be empty.
	OVUM_ASSERT(liveFinder.survivorsFromGen0 == nullptr);
	OVUM_ASSERT(liveFinder.survivorsWithGen0Refs == nullptr);
	OVUM_ASSERT(liveFinder.processList == nullptr);

	// Step 6: Swap white and black for the next cycle and point collectList to
	// the "keep" list.
	std::swap(currentWhite, currentBlack);
	collectList = liveFinder.keepList;
	gen0Current = (char*)gen0Base;

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

void GC::MoveGen0Survivors(LiveObjectFinder &liveFinder)
{
	GCObject **list = &liveFinder.survivorsFromGen0;

	GCObject *obj = *list;
	while (obj)
	{
		GCObject *next = obj->next;

		obj->RemoveFromList(list);
		if (!obj->IsPinned())
		{
			// If the object is not pinned, then move it to gen1.
			MoveSurvivorToGen1(liveFinder, obj);
		}
		else
		{
			// Otherwise, add it to pinnedList.
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

void GC::MoveSurvivorToGen1(LiveObjectFinder &liveFinder, GCObject *gco)
{
	// We can only move to generation 1 from generation 0.
	OVUM_ASSERT((gco->flags & GCOFlags::GENERATION) == GCOFlags::GEN_0);

	size_t objectSize = gco->size;

	GCObject *newAddress = AllocRawGen1(objectSize);
	if (newAddress == nullptr)
		// Not enough available memory to move to generation 1;
		// cannot recover from this.
		abort();

	memcpy(newAddress, gco, objectSize);

	newAddress->flags = (newAddress->flags & ~GCOFlags::GENERATION) | GCOFlags::GEN_1;
	newAddress->InsertIntoList(
		newAddress->HasGen0Refs()
			? &liveFinder.survivorsWithGen0Refs
			: &liveFinder.keepList
	);

	gen1Size += objectSize;
	liveFinder.gen1SurvivorSize += objectSize;

	gco->flags |= GCOFlags::MOVED;
	gco->newAddress = newAddress;

	if (newAddress->type == vm->types.String ||
		newAddress->IsEarlyString())
	{
		String *str = reinterpret_cast<String*>(newAddress->InstanceBase());
		if ((str->flags & StringFlags::INTERN) == StringFlags::INTERN)
			strings.UpdateIntern(str);
	}
}

void GC::UpdateGen0References(LiveObjectFinder &liveFinder)
{
	MovedObjectUpdater updater(this, &liveFinder.keepList);

	// MovedObjectUpdater also visits GC::pinnedList.
	updater.UpdateMovedObjects(liveFinder.survivorsWithGen0Refs);

	// All done with this list!
	liveFinder.survivorsWithGen0Refs = nullptr;
}

void GC::CollectGarbage(LiveObjectFinder &liveFinder, bool collectGen1)
{
	// If collectGen1 is false, we'll still collect generation 1 if there are
	// enough dead objects in it.
	if (!collectGen1)
	{
		size_t deadGen1Size = gen1Size - liveFinder.gen1SurvivorSize;
		collectGen1 = deadGen1Size >= config::Defaults::GEN1_DEAD_OBJECT_THRESHOLD;
	}

	GCObject *item = collectList;
	while (item)
	{
		GCObject *next = item->next;

		if (collectGen1 || (item->flags & GCOFlags::GENERATION) != GCOFlags::GEN_1)
		{
			Release(item);
		}
		else
		{
			// Uncollectible gen1 object, will be collected in the future.
			// Note: We don't have to examine gen0 references or anything like
			// that, because the object is dead.
			item->InsertIntoList(&liveFinder.keepList);
			// Color the object black now, so that it will be white next cycle.
			item->SetColor(currentBlack);
		}

		item = next;
	}

	collectList = nullptr;
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
#if OVUM_DEBUG
		else
		{
			OVUM_ASSERT(!"Failed to insert pinned object into tree; it's probably in the tree already!");
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

} // namespace ovum

OVUM_API int GC_Construct(ThreadHandle thread, TypeHandle type, ovlocals_t argc, Value *output)
{
	return thread->GetGC()->Construct(thread, type, argc, output);
}

OVUM_API String *GC_ConstructString(ThreadHandle thread, int32_t length, const ovchar_t *values)
{
	return thread->GetGC()->ConstructString(thread, length, values);
}

OVUM_API int GC_Alloc(ThreadHandle thread, TypeHandle type, size_t size, Value *output)
{
	return thread->GetGC()->Alloc(thread, type, size, output);
}

OVUM_API int GC_AllocArray(ThreadHandle thread, uint32_t length, size_t itemSize, void **output)
{
	return thread->GetGC()->AllocArray(thread, length, itemSize, output);
}

OVUM_API int GC_AllocValueArray(ThreadHandle thread, uint32_t length, Value **output)
{
	return thread->GetGC()->AllocValueArray(thread, length, output);
}

OVUM_API void GC_AddMemoryPressure(ThreadHandle thread, size_t size)
{
	thread->GetGC()->AddMemoryPressure(thread, size);
}

OVUM_API void GC_RemoveMemoryPressure(ThreadHandle thread, size_t size)
{
	thread->GetGC()->RemoveMemoryPressure(thread, size);
}

OVUM_API Value *GC_AddStaticReference(ThreadHandle thread, Value *initialValue)
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
