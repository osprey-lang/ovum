#include "ov_stringbuffer.h"
#include "aves_list.h"

#define _L(value)    ((value).common.list)

AVES_API void aves_List_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ListInst));
	Type_SetReferenceGetter(type, aves_List_getReferences);
	Type_SetFinalizer(type, aves_List_finalize);
}

int32_t GetIndex(ThreadHandle thread, ListInst *list, Value indexValue)
{
	int64_t index = IntFromValue(thread, indexValue).integer;

	if (index < 0)
		index += list->length;
	if (index < 0 || index >= list->length)
	{
		VM_PushString(thread, strings::index);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	return (int32_t)index;
}

AVES_API NATIVE_FUNCTION(aves_List_new)
{
	InitListInstance(thread, _L(THISV), 0);
}
AVES_API NATIVE_FUNCTION(aves_List_newCap)
{
	int64_t capacity = IntFromValue(thread, args[1]).integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	InitListInstance(thread, _L(THISV), (int32_t)capacity);
}

AVES_API NATIVE_FUNCTION(aves_List_get_length)
{
	VM_PushInt(thread, _L(THISV)->length);
}

AVES_API NATIVE_FUNCTION(aves_List_get_capacity)
{
	VM_PushInt(thread, _L(THISV)->capacity);
}
AVES_API NATIVE_FUNCTION(aves_List_set_capacity)
{
	int64_t capacity = IntFromValue(thread, args[1]).integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	SetListCapacity(thread, _L(THISV), (int32_t)capacity);
}

AVES_API NATIVE_FUNCTION(aves_List_get_version)
{
	ListInst *inst = _L(THISV);
	VM_PushInt(thread, inst->version);
}

AVES_API NATIVE_FUNCTION(aves_List_getItem)
{
	ListInst *list = _L(THISV);
	int32_t index = GetIndex(thread, list, args[1]);

	VM_Push(thread, list->values[index]);
}
AVES_API NATIVE_FUNCTION(aves_List_setItem)
{
	ListInst *list = _L(THISV);
	int32_t index = GetIndex(thread, list, args[1]);

	list->values[index] = args[2];
	list->version++;
}

AVES_API NATIVE_FUNCTION(aves_List_add)
{
	ListInst *list = _L(THISV);

	EnsureMinCapacity(thread, list, list->length + 1);
	list->values[list->length] = args[1];
	list->length++;
	list->version++;
}
AVES_API NATIVE_FUNCTION(aves_List_insert)
{
	int64_t index64 = IntFromValue(thread, args[1]).integer;
	ListInst *list = _L(THISV);
	// when index == list->length, it means we insert at the end
	if (index64 < 0 || index64 > list->length)
	{
		VM_PushString(thread, strings::index);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	int32_t index32 = (int32_t)index64;

	EnsureMinCapacity(thread, list, list->length + 1);

	// Shift all items up by 1
	for (int32_t i = list->length; i > index32; i--)
		list->values[i] = list->values[i - 1];
	list->values[index32] = args[2];
	list->version++;
}
AVES_API NATIVE_FUNCTION(aves_List_removeAt)
{
	ListInst *list = _L(THISV);

	int32_t index = GetIndex(thread, list, args[1]);

	for (int32_t i = (int32_t)index; i < list->length - 1; i++)
		list->values[i] = list->values[i + 1];
	list->length--;
	list->version++;
}
AVES_API NATIVE_FUNCTION(aves_List_clear)
{
	// Setting the length to 0 automatically makes the values inaccessible,
	// even if it doesn't actually delete them. It just makes them free for
	// the GC to reclaim.
	ListInst *list = _L(THISV);
	list->length = 0;
	list->version++;
}
AVES_API NATIVE_FUNCTION(aves_List_slice1)
{
	// slice(start)
	ListInst *list = _L(THISV);

	// Get the start index
	int32_t startIndex = GetIndex(thread, list, args[1]);

	// Create the output list
	Value *outputValue = VM_Local(thread, 0);
	GC_Construct(thread, GetType_List(), 0, outputValue);
	ListInst *outputList = outputValue->common.list;

	int32_t finalLength = list->length - startIndex;
	EnsureMinCapacity(thread, outputList, finalLength);

	// Copy the elements across
	CopyMemoryT(outputList->values, list->values + startIndex, finalLength);
	outputList->length = finalLength;

	// Return the new list
	VM_Push(thread, *outputValue);
}
AVES_API NATIVE_FUNCTION(aves_List_slice2)
{
	// slice(start, end)			(inclusive)
	ListInst *list = _L(THISV);

	// Get the indexes
	int32_t startIndex = GetIndex(thread, list, args[1]);
	int32_t endIndex   = GetIndex(thread, list, args[2]);

	if (endIndex < startIndex)
	{
		VM_PushNull(thread); // paramName
		VM_PushString(thread, error_strings::EndIndexLessThanStart); // message
		GC_Construct(thread, Types::ArgumentRangeError, 2, nullptr);
		VM_Throw(thread);
	}

	// Create the output list
	Value *outputValue = VM_Local(thread, 0);
	GC_Construct(thread, GetType_List(), 0, outputValue);
	ListInst *outputList = outputValue->common.list;

	int32_t finalLength = endIndex - startIndex + 1;
	EnsureMinCapacity(thread, outputList, finalLength);

	// Copy the elements across
	CopyMemoryT(outputList->values, list->values + startIndex, finalLength);
	outputList->length = finalLength;

	// Return the new list
	VM_Push(thread, *outputValue);
}
AVES_API NATIVE_FUNCTION(aves_List_reverse)
{
	// Reverse in-place
	ListInst *list = _L(THISV);
	ReverseArray(list->length, list->values);
	list->version++;
}

AVES_API void InitListInstance(ThreadHandle thread, ListInst *list, const int32_t capacity)
{
	list->capacity = capacity;
	list->length = 0;
	list->version = 0;
	if (capacity == 0)
		list->values = nullptr;
	else
		// Note: we use malloc so that we can later use realloc to resize the list
		list->values = (Value*)malloc(capacity * sizeof(Value));
}

void EnsureMinCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity)
{
	if (list->capacity < capacity)
		SetListCapacity(thread, list, capacity);
}

void SetListCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity)
{
	if (capacity < list->length)
	{
		VM_PushString(thread, strings::capacity);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	list->values = (Value*)realloc(list->values, capacity * sizeof(Value));
	list->capacity = capacity;
	list->version++;
}

bool aves_List_getReferences(void *basePtr, unsigned int &valc, Value **target)
{
	ListInst *list = reinterpret_cast<ListInst*>(basePtr);
	*target = list->values;
	valc = list->length; // Note: not capacity, because we don't know how many have been assigned to
	return false;
}

void aves_List_finalize(ThreadHandle thread, void *basePtr)
{
	ListInst *list = reinterpret_cast<ListInst*>(basePtr);

	delete[] list->values;
	list->length   = 0;
	list->capacity = 0;
	list->version  = -1;
}