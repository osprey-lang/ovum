#include "ov_vm.internal.h"
#include <cstdlib>
#include <cstring>

namespace gc_strings
{
	LitString<47> _ObjectTooBig  = LitString<47>::FromCString("The size of the requested object was too large.");
	LitString<33> _CStringTooBig = LitString<33>::FromCString("GC_ConvertString: input too long.");

	String *ObjectTooBig  = _S(_ObjectTooBig);
	String *CStringTooBig = _S(_CStringTooBig);
}

GCState gcState;

void GC_Init()
{
	GCState state;
	state.isRunning = false;

	state.collectBase = nullptr;
	state.processBase = nullptr;
	state.keepBase = nullptr;

	state.currentCollectMark = 0;
	state.debt = 0;
	state.totalSize = 0;
}


// Note: this method is only visible in this file.
void *InternalAlloc(size_t size)
{
	assert(size >= sizeof(GCObject));
	return malloc(size);
}

// Note: this method is only visible in this file.
void InternalRelease(GCObject *gco)
{
	free(gco);
}

void GC_Alloc(Thread *const thread, const Type *type, size_t size, GCObject **output)
{
	if (SIZE_MAX - size < sizeof(GCObject))
		thread->ThrowMemoryError(gc_strings::ObjectTooBig);

	size += sizeof(GCObject);
	GCObject *gco = (GCObject*)InternalAlloc(size);

	GCState &gc = gcState;

	if (!gco) // Allocation failed (we're probably out of memory)
	{
		// Allocation may happen during collection, in which case we don't do anything.
		if (!gc.isRunning)
		{
			// Note that we do not need to do anything to preserve the gco,
			// because we don't actually have a reference to anything.
			GC_Collect(thread);  // Try to free some memory
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
	gco->flags = gc.isRunning ? GCO_KEEP(gc.currentCollectMark) : GCO_COLLECT(gc.currentCollectMark);
	GC_InsertIntoList(gco, gc.isRunning ? &gc.keepBase : &gc.collectBase);

	// These should never overflow unless we forget to reset/decrement them,
	// because it should not be possible to allocate more than SIZE_MAX bytes.
	gcState.debt += size > GC_LARGE_OBJECT_SIZE ? GC_LARGE_OBJECT_SIZE : size;
	gcState.totalSize += size;

	*output = gco;

	// There is no managed reference to the object yet, so if collection is necessary,
	// we need to move the object to the Keep list, or the object will be collected.
	if (!gcState.isRunning && gcState.debt >= GC_MAX_DEBT)
	{
		GC_RemoveFromList(gco, &gcState.collectBase);
		GC_InsertIntoList(gco, &gcState.keepBase);
		GC_Collect(thread);
	}
}

OVUM_API void GC_Construct(ThreadHandle thread, TypeHandle type, const uint16_t argc, Value *output)
{
	Thread *const _thread = _Th(thread);
	const Type *_type = _Tp(type);
	if (type == stdTypes.String || _type->flags & (TYPE_PRIMITIVE | TYPE_ABSTRACT))
		_thread->ThrowTypeError();

	GCObject *gco;
	GC_Alloc(_thread, _type, _type->fieldsOffset + _type->size, &gco);

	Value value;
	value.type = type;
	value.instance = GCO_INSTANCE_BASE(gco);

	StackFrame *frame = _thread->currentFrame;
	Value *framePointer = frame->evalStack + frame->stackCount;

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
	_thread->InvokeMember(static_strings::_new, argc, &ignore);
	if (output)
		*output = value;
	else
		frame->Push(value);
}

OVUM_API void GC_ConstructString(ThreadHandle thread, const int32_t length, const uchar value[], String **output)
{
	Thread *const _thread = _Th(thread);
	GCObject *gco;
	// Note: sizeof(STRING) includes firstChar, but we need an extra character
	// for the terminating \0 anyway. So this is fine.
	GC_Alloc(_Th(thread), _Tp(stdTypes.String), sizeof(String) + length*sizeof(uchar), &gco);

	MutableString *str = reinterpret_cast<MutableString*>(GCO_INSTANCE_BASE(gco));
	str->length = length;
	// Note: Alloc() initializes the bytes to 0. The default values of
	// hashCode and flags are both 0, so we don't need to set either here.

	// If you pass a null value, you get a string with nothing but \0s.
	if (value != nullptr)
		// Note: this does NOT include the terminating \0, which is fine.
		CopyMemoryT(&str->firstChar, value, length);

	*output = reinterpret_cast<String*>(str);
}

OVUM_API String *GC_ConvertString(ThreadHandle thread, const char *string)
{
	size_t length = strlen(string);

	if (length > INT32_MAX)
		_Th(thread)->ThrowOverflowError(gc_strings::CStringTooBig);

	String *output;
	GC_ConstructString(thread, length, nullptr, &output);

	if (length > 0)
	{
		uchar *mutch = const_cast<uchar*>(&output->firstChar);
		while (*string)
			*mutch++ = *string++;
	}

	return output;
}


OVUM_API void GC_AddMemoryPressure(ThreadHandle thread, const size_t size)
{
	throw L"Not implemented";
}
OVUM_API void GC_RemoveMemoryPressure(ThreadHandle thread, const size_t size)
{
	throw L"Not implemented";
}


void GC_Release(Thread *const thread, GCObject *gco)
{
	assert((gco->flags & GCO_COLLECT(gcState.currentCollectMark)) == GCO_COLLECT(gcState.currentCollectMark));

	const Type *type = gco->type;
	while (type)
	{
		if (type->finalizer)
			type->finalizer(thread, INST_FROM_GCO(gco, type));
		type = type->baseType;
	}

	gcState.totalSize -= gco->size; // gco->size includes the size of the GCOBJECT
	InternalRelease(gco); // goodbye, dear pointer.
}

void GC_Collect(Thread *const thread)
{
	// Upon entering this method, all objects are in collectBase.
	// Step 1: move all the root objects to the Process list.
	GC_MarkRootSet();

	GCState &gc = gcState;

	// Step 2: examine all objects in the Process list.
	// Using the type information in each GCOBJECT, we can figure
	// out what an instance points to! 
	// Note: objects are added to the beginning of the list.
	while (gc.processBase)
	{
		GCObject *item = gc.processBase;
		do
		{
			GC_ProcessObjectAndFields(item);
		} while (item = item->next);
	}
	assert(gc.processBase == nullptr);
	// Step 3: free all objects left in the Collect list.
	while (gc.collectBase)
	{
		GCObject *next = gc.collectBase->next;
		GC_Release(thread, gc.collectBase);
		gc.collectBase = next;
	}
	// Step 4: reset the debt.
	// NOTE potential problem: this disregards any objects
	// that were allocated during collection, e.g. as part
	// of finalizers or whatever.
	gc.debt = 0;
	// Step 5: increment currentCollectMark for next cycle
	// and set current Keep list to Collect.
	gc.currentCollectMark = (gc.currentCollectMark + 2) % 3;
	gc.collectBase = gc.keepBase;
	gc.keepBase = nullptr;
}

void GC_MarkRootSet()
{
	Thread *const mainThread = vmState.mainThread;

	// Mark stack frames first.
	StackFrame *frame = mainThread->currentFrame;
	// Frames are marked top-to-bottom.
	do
	{
		Method::Overload *method = frame->method;
		// Does the method have any locals?
		if (method->paramCount)
			GC_ProcessFields(method->paramCount, frame->arguments);
		// By design, the locals and the eval stack are adjacent in memory.
		// Hence, the following is safe:
		if (method->locals || frame->stackCount)
			GC_ProcessFields(method->locals + frame->stackCount, LOCALS_OFFSET(frame));

		frame = frame->prevFrame;
	} while (frame = frame->prevFrame);

	// We need to do this because the GC may be triggered in
	// a finally clause, and we wouldn't want to obliterate
	// the error if we still need to catch it later, right?
	if (GC_ShouldProcess(mainThread->currentError))
		GC_Process(GCO_FROM_VALUE(mainThread->currentError));

	// TODO: mark static members
}

void GC_ProcessObjectAndFields(GCObject *gco)
{
	// The object is not supposed to be anything but GCO_PROCESS at this point.
	assert(gco->flags & GCO_PROCESS(gcState.currentCollectMark));
	// It's also not supposed to be a value type.
	assert((gco->type->flags & TYPE_PRIMITIVE) == TYPE_NONE);

	GC_Keep(gco);

	const Type *type = gco->type;
	while (type)
	{
		if (type->flags & TYPE_CUSTOMPTR)
			GC_ProcessCustomFields(type, gco);
		else if (type->fieldCount)
			GC_ProcessFields(type->fieldCount, (Value*)INST_FROM_GCO(gco,type));

		type = type->baseType;
	}
}

void GC_ProcessCustomFields(const Type *type, GCObject *gco)
{
	if (type == stdTypes.Hash)
	{
		GC_ProcessHash((HashInst*)(gco + 1));
	}
	else if (type == stdTypes.List)
	{
		ListInst *list = (ListInst*)(gco + 1);
		GC_ProcessFields(list->length, list->values);
	}
	else if (type == stdTypes.Method)
	{
		MethodInst *minst = (MethodInst*)(gco + 1);
		if (minst->instance.type)
			GC_ProcessFields(1, &minst->instance);
	}
	else
	{
		unsigned int fieldCount;
		Value *fields;
		bool deleteAfter = type->getReferences(INST_FROM_GCO(gco, type), fieldCount, &fields);

		GC_ProcessFields(fieldCount, fields);

		if (deleteAfter)
			delete[] fields;
	}
}

void GC_ProcessHash(HashInst *hash)
{
	int32_t entryCount = hash->capacity;
	HashEntry *entries = hash->entries;

	for (int32_t i = 0; i < entryCount; i++)
		if (entries[i].hashCode != -1)
		{
			HashEntry *entry = entries + i;

			if (GC_ShouldProcess(entry->key))
				GC_Process(GCO_FROM_VALUE(entry->key));

			if (GC_ShouldProcess(entry->value))
				GC_Process(GCO_FROM_VALUE(entry->value));
		}
}