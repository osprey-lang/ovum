#include "aves_list.h"
#include <cstddef>
#include <cassert>

#define _L(value)    ((value).common.list)

AVES_API void aves_List_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ListInst));

	Type_AddNativeField(type, offsetof(ListInst, values), NativeFieldType::GC_ARRAY);
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

	PinnedAlias<ListInst> list(THISP);
	CHECKED(SetListCapacity(thread, *list, (int32_t)capacity));
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

	VM_Push(thread, &list->values[index]);
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
	PinnedAlias<ListInst> list(THISP);

	CHECKED(EnsureMinCapacity(thread, *list, list->length + 1));
	list->values[list->length] = args[1];
	list->length++;
	list->version++;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_insert)
{
	PinnedAlias<ListInst> list(THISP);
	// when index == list->length, it means we insert at the end
	int32_t index;
	CHECKED(GetIndex(thread, *list, args[1], true, index));

	CHECKED(EnsureMinCapacity(thread, *list, list->length + 1));

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

	int32_t i;
	for (i = (int32_t)index; i < list->length - 1; i++)
		list->values[i] = list->values[i + 1];
	list->values[i].type = nullptr;
	list->length--;
	list->version++;
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_List_clear)
{
	ListInst *list = _L(THISV);
	for (int32_t i = 0; i < list->length; i++)
		list->values[i].type = nullptr;
	list->length = 0;
	list->version++;
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_slice1)
{
	// slice(start)
	PinnedAlias<ListInst> list(THISP);

	// Get the start index
	int32_t startIndex;
	CHECKED(GetIndex(thread, *list, args[1], false, startIndex));

	// Create the output list
	Value *output = VM_Local(thread, 0);
	CHECKED(SliceList(thread, *list, startIndex, list->length, output));

	// Return the new list
	VM_Push(thread, output);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_List_slice2)
{
	// slice(start inclusive, end exclusive)
	PinnedAlias<ListInst> list(THISP);

	// Get the indexes
	int32_t startIndex, endIndex;
	CHECKED(GetIndex(thread, *list, args[1], true, startIndex));
	CHECKED(GetIndex(thread, *list, args[2], true, endIndex));

	Value *output = VM_Local(thread, 0);

	CHECKED(SliceList(thread, *list, startIndex, endIndex, output));

	// Return the new list
	VM_Push(thread, output);
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
		PinnedAlias<ListInst> p(list);

		int r = GC_AllocValueArray(thread, (uint32_t)capacity, &list->values);
		if (r != OVUM_SUCCESS) return r;
	}
	RETURN_SUCCESS;
}

int EnsureMinCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity)
{
	if (list->capacity < capacity)
		// Try to double the capacity, but make sure we can always satisfy
		// the requested minimum capacity.
		return SetListCapacity(thread, list, max(list->capacity * 2, capacity));
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

	Value *newValues;
	int r = GC_AllocValueArray(thread, capacity, &newValues);
	if (r != OVUM_SUCCESS) return r;

	CopyMemoryT(newValues, list->values, list->length);

	list->values = newValues;
	list->capacity = capacity;
	list->version++;
	RETURN_SUCCESS;
}

int SliceList(ThreadHandle thread, ListInst *list, int32_t startIndex, int32_t endIndex, Value *output)
{
	int __status;

	if (endIndex < startIndex)
	{
		VM_PushNull(thread); // paramName
		VM_PushString(thread, error_strings::EndIndexLessThanStart); // message
		CHECKED(GC_Construct(thread, Types::ArgumentRangeError, 2, nullptr));
		return VM_Throw(thread);
	}

	int32_t sliceLength = endIndex - startIndex;

	// Create the output list
	VM_PushInt(thread, sliceLength); // capacity
	CHECKED(GC_Construct(thread, GetType_List(), 1, output));

	if (sliceLength > 0)
	{
		ListInst *outputList = output->common.list;
		assert(outputList->capacity >= sliceLength);

		// Copy the elements across
		CopyMemoryT(outputList->values, list->values + startIndex, sliceLength);
		outputList->length = sliceLength;
	}

	// Return the new list
	VM_Push(thread, output);

	__status = OVUM_SUCCESS;
__retStatus:
	return __status;
}