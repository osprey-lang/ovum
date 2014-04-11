#include "ov_stringbuffer.h"
#include "aves_list.h"

#define _L(value)    ((value).common.list)

AVES_API void aves_List_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ListInst));
	Type_SetReferenceGetter(type, aves_List_getReferences);
	Type_SetFinalizer(type, aves_List_finalize);
}

int GetIndex(ThreadHandle thread, ListInst *list, Value indexValue, bool canEqualLength, int32_t &result)
{
	int r = IntFromValue(thread, &indexValue);
	if (r != OVUM_SUCCESS) return r;
	int64_t index = indexValue.integer;

	if (index < 0)
		index += list->length;
	if (index < 0 || (canEqualLength ? index > list->length : index >= list->length))
	{
		VM_PushString(thread, strings::index);
		r = GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		if (r == OVUM_SUCCESS)
			r = VM_Throw(thread);
		return r;
	}

	result = (int32_t)index;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_List_new)
{
	return InitListInstance(thread, _L(THISV), 0);
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_newCap)
{
	CHECKED(IntFromValue(thread, args + 1));
	int64_t capacity = args[1].integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		CHECKED(GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr));
		return VM_Throw(thread);
	}

	CHECKED(InitListInstance(thread, _L(THISV), (int32_t)capacity));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_List_get_length)
{
	VM_PushInt(thread, _L(THISV)->length);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_List_get_capacity)
{
	VM_PushInt(thread, _L(THISV)->capacity);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_set_capacity)
{
	CHECKED(IntFromValue(thread, args + 1));
	int64_t capacity = args[1].integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		CHECKED(GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr));
		return VM_Throw(thread);
	}

	CHECKED(SetListCapacity(thread, _L(THISV), (int32_t)capacity));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_List_get_version)
{
	ListInst *inst = _L(THISV);
	VM_PushInt(thread, inst->version);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_get_item)
{
	ListInst *list = _L(THISV);
	int32_t index;
	CHECKED(GetIndex(thread, list, args[1], false, index));

	VM_Push(thread, list->values[index]);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_set_item)
{
	ListInst *list = _L(THISV);
	int32_t index;
	CHECKED(GetIndex(thread, list, args[1], false, index));

	list->values[index] = args[2];
	list->version++;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_add)
{
	ListInst *list = _L(THISV);

	CHECKED(EnsureMinCapacity(thread, list, list->length + 1));
	list->values[list->length] = args[1];
	list->length++;
	list->version++;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_insert)
{
	ListInst *list = _L(THISV);
	// when index == list->length, it means we insert at the end
	int32_t index;
	CHECKED(GetIndex(thread, list, args[1], true, index));

	CHECKED(EnsureMinCapacity(thread, list, list->length + 1));

	// Shift all items up by 1
	for (int32_t i = list->length; i > index; i--)
		list->values[i] = list->values[i - 1];
	list->values[index] = args[2];
	list->length++;
	list->version++;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_removeAt)
{
	ListInst *list = _L(THISV);

	int32_t index;
	CHECKED(GetIndex(thread, list, args[1], false, index));

	for (int32_t i = (int32_t)index; i < list->length - 1; i++)
		list->values[i] = list->values[i + 1];
	list->length--;
	list->version++;
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_List_clear)
{
	// Setting the length to 0 automatically makes the values inaccessible,
	// even if it doesn't actually delete them. It just makes them free for
	// the GC to reclaim.
	ListInst *list = _L(THISV);
	list->length = 0;
	list->version++;
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_slice1)
{
	// slice(start)
	Alias<ListInst> list(THISP);

	// Get the start index
	int32_t startIndex;
	CHECKED(GetIndex(thread, *list, args[1], false, startIndex));

	int32_t sliceLength = list->length - startIndex;

	// Create the output list
	Value *outputValue = VM_Local(thread, 0);
	VM_PushInt(thread, sliceLength); // capacity
	CHECKED(GC_Construct(thread, GetType_List(), 1, outputValue));

	ListInst *outputList = outputValue->common.list;
	assert(outputList->capacity >= sliceLength);

	// Copy the elements across
	CopyMemoryT(outputList->values, list->values + startIndex, sliceLength);
	outputList->length = sliceLength;

	// Return the new list
	VM_Push(thread, *outputValue);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_slice2)
{
	// slice(start inclusive, end exclusive)
	Alias<ListInst> list(THISP);

	// Get the indexes
	int32_t startIndex, endIndex;
	CHECKED(GetIndex(thread, *list, args[1], true, startIndex));
	CHECKED(GetIndex(thread, *list, args[2], true, endIndex));

	if (endIndex < startIndex)
	{
		VM_PushNull(thread); // paramName
		VM_PushString(thread, error_strings::EndIndexLessThanStart); // message
		CHECKED(GC_Construct(thread, Types::ArgumentRangeError, 2, nullptr));
		return VM_Throw(thread);
	}

	int32_t sliceLength = endIndex - startIndex;

	// Create the output list
	Value *outputValue = VM_Local(thread, 0);
	VM_PushInt(thread, sliceLength); // capacity
	CHECKED(GC_Construct(thread, GetType_List(), 1, outputValue));

	ListInst *outputList = outputValue->common.list;
	assert(outputList->capacity >= sliceLength);

	// Copy the elements across
	CopyMemoryT(outputList->values, list->values + startIndex, sliceLength);
	outputList->length = sliceLength;

	// Return the new list
	VM_Push(thread, *outputValue);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_List_reverse)
{
	// Reverse in-place
	ListInst *list = _L(THISV);
	ReverseArray(list->length, list->values);
	list->version++;
	RETURN_SUCCESS;
}

AVES_API int InitListInstance(ThreadHandle thread, ListInst *list, const int32_t capacity)
{
	list->capacity = capacity;
	list->length = 0;
	list->version = 0;
	if (capacity == 0)
		list->values = nullptr;
	else
	{
		// Note: we use malloc so that we can later use realloc to resize the list
		list->values = (Value*)malloc(capacity * sizeof(Value));
		if (list->values == nullptr)
			return OVUM_ERROR_NO_MEMORY;
	}
	RETURN_SUCCESS;
}

int EnsureMinCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity)
{
	if (list->capacity < capacity)
		return SetListCapacity(thread, list, capacity);
	RETURN_SUCCESS;
}

int SetListCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity)
{
	if (capacity < list->length)
	{
		VM_PushString(thread, strings::capacity);
		int r = GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		if (r == OVUM_SUCCESS)
			r = VM_Throw(thread);
		return r;
	}

	Value *newValues = (Value*)realloc(list->values, capacity * sizeof(Value));
	if (newValues == nullptr)
		return VM_ThrowMemoryError(thread);
	list->values = newValues;
	list->capacity = capacity;
	list->version++;
	RETURN_SUCCESS;
}

bool aves_List_getReferences(void *basePtr, unsigned int *valc, Value **target, int32_t *state)
{
	ListInst *list = reinterpret_cast<ListInst*>(basePtr);
	*target = list->values;
	*valc = list->length; // Note: not capacity, because we don't know how many have been assigned to
	return false;
}

void aves_List_finalize(void *basePtr)
{
	ListInst *list = reinterpret_cast<ListInst*>(basePtr);

	free(list->values);
	list->length   = 0;
	list->capacity = 0;
	list->version  = -1;
}