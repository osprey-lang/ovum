#include "ov_vm.internal.h"
#include "ov_module.internal.h"
#include "ov_debug_symbols.internal.h"
#include <cstdlib>
#include <cstring>
#include <vector>

namespace gc_strings
{
	LitString<47> _ObjectTooBig  = LitString<47>::FromCString("The size of the requested object was too large.");
	LitString<33> _CStringTooBig = LitString<33>::FromCString("GC_ConvertString: input too long.");

	String *ObjectTooBig  = _S(_ObjectTooBig);
	String *CStringTooBig = _S(_CStringTooBig);
}

GC *GC::gc;

void GC::Init()
{
	GC::gc = new GC();
}

void GC::Unload()
{
	delete GC::gc;
}

GC::GC() :
	isRunning(false),
	collectBase(nullptr), processBase(nullptr), keepBase(nullptr),
	currentCollectMark(0), debt(0), totalSize(0),
	collectCount(0), strings(32), staticRefs(nullptr)
{ }

GC::~GC()
{
	GCObject *gco = collectBase;
	while (gco)
	{
		GCObject *next = gco->next;
		Release(VM::vm->mainThread, gco);
		gco = next;
	}

	collectBase = nullptr;

	StaticRefBlock *refs = staticRefs;
	while (refs)
	{
		StaticRefBlock *next = refs->next;
		delete refs;
		refs = next;
	}
}


void *GC::InternalAlloc(size_t size)
{
	assert(size >= GCO_SIZE);
	return malloc(size);
}

void GC::InternalRelease(GCObject *gco)
{
	free(gco);
}

void GC::Alloc(Thread *const thread, Type *type, size_t size, GCObject **output)
{
	if (SIZE_MAX - size < GCO_SIZE)
		thread->ThrowMemoryError(gc_strings::ObjectTooBig);

	size += GCO_SIZE;
	GCObject *gco = (GCObject*)InternalAlloc(size);

	if (!gco) // Allocation failed (we're probably out of memory)
	{
		// Allocation may happen during collection, in which case we don't do anything.
		if (!isRunning)
		{
			// Note that we do not need to do anything to preserve the gco,
			// because we don't actually have a reference to anything.
			Collect(thread);  // Try to free some memory
			gco = (GCObject*)InternalAlloc(size); // And allocate again...
		}
		if (!gco)
		{
			// It is not possible to recover from an out-of-memory error.
			// To avoid potential problems with finalizers allocating memory,
			// abort() is used instead of exit().
			abort();
		}
	}

	memset(gco, 0, size);
	gco->size = size;
	gco->type = type;
	// If the GC is currently running, then do not collect the new GCO.
	// Otherwise, put it in collectBase. It won't be collected until the next cycle.
	gco->flags = isRunning ? GCO_KEEP(currentCollectMark) : GCO_COLLECT(currentCollectMark);
	InsertIntoList(gco, isRunning ? &keepBase : &collectBase);

	// These should never overflow unless we forget to reset/decrement them,
	// because it should not be possible to allocate more than SIZE_MAX bytes.
	debt += size > GC_LARGE_OBJECT_SIZE ? GC_LARGE_OBJECT_SIZE : size;
	totalSize += size;

	*output = gco;

	// There is no managed reference to the object yet, so if collection is necessary,
	// we need to move the object to the Keep list, or the object will be collected.
	if (!isRunning && debt >= GC_MAX_DEBT)
	{
		RemoveFromList(gco, &collectBase);
		InsertIntoList(gco, &keepBase);
		MARK_GCO(gco, GCO_KEEP(currentCollectMark));
		Collect(thread);
	}
}

void GC::Construct(Thread *const thread, Type *type, const uint16_t argc, Value *output)
{
	if (type == VM::vm->types.String ||
		(type->flags & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE ||
		(type->flags & TypeFlags::ABSTRACT) == TypeFlags::ABSTRACT)
		thread->ThrowTypeError();

	StackFrame *frame = thread->currentFrame;
	ConstructLL(thread, type, argc, frame->evalStack + frame->stackCount - argc, output);
}

void GC::ConstructLL(Thread *const thread, Type *type, const uint16_t argc, Value *args, Value *output)
{
	GCObject *gco;
	Alloc(thread, type, type->fieldsOffset + type->size, &gco);

	Value value;
	value.type = type;
	value.instance = (uint8_t*)GCO_INSTANCE_BASE(gco);

	StackFrame *frame = thread->currentFrame;
	Value *framePointer = args + argc;

	// Unshift value onto beginning of eval stack! Hurrah.
	// If argc == 0, the loop doesn't run.
	for (int i = 0; i < argc; i++)
	{
		*framePointer = *(framePointer - 1);
		framePointer--;
	}

	*framePointer = value;
	frame->stackCount++;

	Value ignore; // all Ovum methods return values, even the constructor
	thread->InvokeMember(static_strings::_new, argc, &ignore);
	if (output)
		*output = value;
	else
		frame->Push(value);
}

String *GC::ConstructString(Thread *const thread, const int32_t length, const uchar value[])
{
	GCObject *gco;
	// Note: sizeof(String) includes firstChar, but we need an extra character
	// for the terminating \0 anyway. So this is fine.
	Alloc(thread, VM::vm->types.String, sizeof(String) + length*sizeof(uchar), &gco);
	if (VM::vm->types.String == nullptr)
		gco->flags |= GCOFlags::EARLY_STRING;

	MutableString *str = reinterpret_cast<MutableString*>(GCO_INSTANCE_BASE(gco));
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
		thread->ThrowOverflowError(gc_strings::CStringTooBig);

	String *output = ConstructString(thread, length, nullptr);

	if (length > 0)
	{
		uchar *mutch = const_cast<uchar*>(&output->firstChar);
		while (*string)
			*mutch++ = *string++;
	}

	return output;
}


void GC::AddMemoryPressure(Thread *const thread, const size_t size)
{
	// Not implemented yet
}
void GC::RemoveMemoryPressure(Thread *const thread, const size_t size)
{
	// Not implemented yet
}

StaticRef *GC::AddStaticReference(Value value)
{
	if (staticRefs == nullptr ||
		staticRefs->count == StaticRefBlock::BLOCK_SIZE)
		staticRefs = new StaticRefBlock(staticRefs);

	StaticRef *output = staticRefs->values + staticRefs->count++;
	output->Init(value);
	return output;
}

void GC::Release(Thread *const thread, GCObject *gco)
{
	assert((gco->flags & GCOFlags::MARK) == GCO_COLLECT(currentCollectMark));

	if ((gco->flags & GCOFlags::EARLY_STRING) != GCOFlags::NONE ||
		gco->type == VM::vm->types.String)	
	{
		String *str = reinterpret_cast<String*>(GCO_INSTANCE_BASE(gco));
		if ((str->flags & StringFlags::INTERN) != StringFlags::NONE)
			strings.RemoveIntern(str);
	}
	else if (gco->type->HasFinalizer())
	{
		Type *type = gco->type;
		do
		{
			if (type->finalizer)
				type->finalizer(thread, INST_FROM_GCO(gco, type));
		} while (type = type->baseType);
	}

	totalSize -= gco->size; // gco->size includes the size of the GCOBJECT
	InternalRelease(gco); // goodbye, dear pointer.
}

void GC::Collect(Thread *const thread)
{
	collectCount++;
	// Upon entering this method, all objects are in collectBase.
	// Step 1: move all the root objects to the Process list.
	MarkRootSet();

	// Step 2: examine all objects in the Process list.
	// Using the type information in each GCOBJECT, we can figure
	// out what an instance points to!
	// Note: objects are added to the beginning of the list.
	while (processBase)
	{
		GCObject *item = processBase;
		do
		{
			GCObject *next = item->next;
			ProcessObjectAndFields(item);
			item = next;
		} while (item);
	}
	assert(processBase == nullptr);

#ifdef PRINT_DEBUG_INFO
	wprintf(L"Preparing to collect #: %d\n", LinkedListLength(collectBase));
#endif

	// Step 3: free all objects left in the Collect list.
	//GCObject *collect = collectBase;
	while (collectBase)
	{
		GCObject *next = collectBase->next;
		if ((collectBase->flags & GCOFlags::IMMORTAL) != GCOFlags::NONE)
			Keep(collectBase);
		else
			Release(thread, collectBase);
		collectBase = next;
	}
	// Step 4: reset the debt.
	// NOTE potential problem: this disregards any objects
	// that were allocated during collection, e.g. as part
	// of finalizers or whatever.
	debt = 0;
	// Step 5: increment currentCollectMark for next cycle
	// and set current Keep list to Collect.
	currentCollectMark = (currentCollectMark + 2) % 3;
	collectBase = keepBase;
	keepBase = nullptr;
}

void GC::MarkRootSet()
{
	Thread *const mainThread = VM::vm->mainThread;

	// Mark stack frames first.
	StackFrame *frame = mainThread->currentFrame;
	// Frames are marked top-to-bottom.
	while (frame)
	{
		Method::Overload *method = frame->method;
		// Does the method have any parameters?
		unsigned int paramCount = method->GetEffectiveParamCount();
		if (paramCount)
			ProcessFields(paramCount, (Value*)frame - paramCount);
		// By design, the locals and the eval stack are adjacent in memory.
		// Hence, the following is safe:
		if (method->locals || frame->stackCount)
			ProcessFields(method->locals + frame->stackCount, LOCALS_OFFSET(frame));

		frame = frame->prevFrame;
	}

	// We need to do this because the GC may be triggered in
	// a finally clause, and we wouldn't want to obliterate
	// the error if we still need to catch it later, right?
	TryProcess(mainThread->currentError);

	// Examine module strings! We don't want to collect these, even
	// if there is nothing referencing them anywhere else.
	for (int i = 0; i < Module::loadedModules->GetLength(); i++)
	{
		Module *m = Module::loadedModules->Get(i);
		TryProcessString(m->name);

		for (int32_t s = 0; s < m->strings.GetLength(); s++)
			TryProcessString(m->strings[s]);

		if (m->debugData)
		{
			debug::ModuleDebugData *debug = m->debugData;
			for (int32_t f = 0; f < debug->fileCount; f++)
				TryProcessString(debug->files[f].fileName);
		}
	}

	// And then we have all the beautiful, lovely static references.
	StaticRefBlock *staticRefs = this->staticRefs;
	while (staticRefs)
	{
		for (unsigned int i = 0; i < staticRefs->count; i++)
			TryProcess(&staticRefs->values[i].value);
		staticRefs = staticRefs->next;
	}
}

void GC::ProcessObjectAndFields(GCObject *gco)
{
	// The object is not supposed to be anything but GCO_PROCESS at this point.
	assert((gco->flags & GCOFlags::MARK) == GCO_PROCESS(currentCollectMark));
	// It's also not supposed to be a value type.
	assert(!gco->type || (gco->type->flags & TypeFlags::PRIMITIVE) != TypeFlags::PRIMITIVE);

	Keep(gco);

	Type *type = gco->type;
	while (type)
	{
		if ((type->flags & TypeFlags::CUSTOMPTR) != TypeFlags::NONE)
			ProcessCustomFields(type, gco);
		else if (type->fieldCount)
			ProcessFields(type->fieldCount, (Value*)INST_FROM_GCO(gco, type));

		type = type->baseType;
	}
}

void GC::ProcessCustomFields(Type *type, GCObject *gco)
{
	if (type == VM::vm->types.Hash)
	{
		ProcessHash((HashInst*)GCO_INSTANCE_BASE(gco));
	}
	else if (type == VM::vm->types.List)
	{
		ListInst *list = (ListInst*)GCO_INSTANCE_BASE(gco);
		ProcessFields(list->length, list->values);
	}
	else if (type == VM::vm->types.Method)
	{
		MethodInst *minst = (MethodInst*)GCO_INSTANCE_BASE(gco);
		if (minst->instance.type)
			TryProcess(&minst->instance);
	}
	else if (type->getReferences) // If the type has no reference getter, assume it has no managed references
	{
		unsigned int fieldCount;
		Value *fields;
		bool deleteAfter = type->getReferences(INST_FROM_GCO(gco, type), fieldCount, &fields);

		ProcessFields(fieldCount, fields);

		if (deleteAfter)
			delete[] fields;
	}
}

void GC::ProcessHash(HashInst *hash)
{
	int32_t entryCount = hash->count;
	HashEntry *entries = hash->entries;

	for (int32_t i = 0; i < entryCount; i++)
		if (entries[i].hashCode >= 0)
		{
			HashEntry *entry = entries + i;
			TryProcess(&entry->key);
			TryProcess(&entry->value);
		}
}


OVUM_API void GC_Construct(ThreadHandle thread, TypeHandle type, const uint16_t argc, Value *output)
{
	GC::gc->Construct(thread, type, argc, output);
}

OVUM_API String *GC_ConstructString(ThreadHandle thread, const int32_t length, const uchar *values)
{
	return GC::gc->ConstructString(thread, length, values);
}

OVUM_API String *GC_ConvertString(ThreadHandle thread, const char *string)
{
	return GC::gc->ConvertString(thread, string);
}

OVUM_API void GC_AddMemoryPressure(ThreadHandle thread, const size_t size)
{
	GC::gc->AddMemoryPressure(thread, size);
}

OVUM_API void GC_RemoveMemoryPressure(ThreadHandle thread, const size_t size)
{
	GC::gc->RemoveMemoryPressure(thread, size);
}

OVUM_API Value *GC_AddStaticReference(Value initialValue)
{
	return GC::gc->AddStaticReference(initialValue)->GetValuePointer();
}

OVUM_API void GC_Collect(ThreadHandle thread)
{
	GC::gc->Collect(thread);
}