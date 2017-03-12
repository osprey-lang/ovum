#include "list.h"
#include "../aves_state.h"
#include <cstddef>

using namespace aves;

AVES_API int OVUM_CDECL aves_List_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ListInst));

	int r = Type_AddNativeField(type, offsetof(ListInst, values), NativeFieldType::GC_ARRAY);
	if (r != OVUM_SUCCESS)
		return r;
	RETURN_SUCCESS;
}

int GetIndex(ThreadHandle thread, ListInst *list, Value *indexValue, bool canEqualLength, size_t &result)
{
	Aves *aves = Aves::Get(thread);

	int r = IntFromValue(thread, indexValue);
	if (r != OVUM_SUCCESS) return r;
	int64_t index = indexValue->v.integer;

	if (index < 0 || (canEqualLength ? index > list->length : index >= list->length))
	{
		VM_PushString(thread, strings::index);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	result = (size_t)index;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_List_new)
{
	return InitListInstance(thread, THISV.v.list, 0);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_newCap)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));
	int64_t capacity = args[1].v.integer;
	if (capacity < 0 || capacity > OVUM_ISIZE_MAX)
	{
		VM_PushString(thread, strings::capacity);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	CHECKED(InitListInstance(thread, THISV.v.list, (size_t)capacity));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_List_get_length)
{
	VM_PushInt(thread, THISV.v.list->length);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_List_get_capacity)
{
	VM_PushInt(thread, THISV.v.list->capacity);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_set_capacity)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));
	int64_t capacity = args[1].v.integer;
	if (capacity < 0 || capacity > OVUM_ISIZE_MAX)
	{
		VM_PushString(thread, strings::capacity);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	PinnedAlias<ListInst> list(THISP);
	CHECKED(SetListCapacity(thread, *list, (size_t)capacity));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_List_get_version)
{
	ListInst *inst = THISV.v.list;
	VM_PushInt(thread, inst->version);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_get_item)
{
	ListInst *list = THISV.v.list;
	size_t index;
	CHECKED(GetIndex(thread, list, args + 1, false, index));

	VM_Push(thread, &list->values[index]);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_set_item)
{
	ListInst *list = THISV.v.list;
	size_t index;
	CHECKED(GetIndex(thread, list, args + 1, false, index));

	list->values[index] = args[2];
	list->version++;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_add)
{
	PinnedAlias<ListInst> list(THISP);

	size_t newIndex = list->length;

	CHECKED(EnsureMinCapacity(thread, *list, list->length + 1));
	list->values[newIndex] = args[1];
	list->length++;
	list->version++;

	VM_PushInt(thread, (int64_t)newIndex);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_insert)
{
	PinnedAlias<ListInst> list(THISP);
	// when index == list->length, it means we insert at the end
	size_t index;
	CHECKED(GetIndex(thread, *list, args + 1, true, index));

	CHECKED(EnsureMinCapacity(thread, *list, list->length + 1));

	// Shift all items up by 1
	for (size_t i = list->length; i > index; i--)
		list->values[i] = list->values[i - 1];
	list->values[index] = args[2];
	list->length++;
	list->version++;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_removeAt)
{
	ListInst *list = THISV.v.list;

	size_t index;
	CHECKED(GetIndex(thread, list, args + 1, false, index));

	size_t i;
	for (i = index; i < list->length - 1; i++)
		list->values[i] = list->values[i + 1];
	list->values[i].type = nullptr;
	list->length--;
	list->version++;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_List_clear)
{
	ListInst *list = THISV.v.list;
	for (size_t i = 0; i < list->length; i++)
		list->values[i].type = nullptr;
	list->length = 0;
	list->version++;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_concatInternal)
{
	// concatInternal(other: List)

	Value *result = VM_Local(thread, 0);

	PinnedAlias<ListInst> la(THISP), lb(args + 1);

	size_t totalLength = la->length + lb->length;
	VM_PushInt(thread, (int64_t)totalLength);
	CHECKED(GC_Construct(thread, GetType_List(thread), 1, result));

	ListInst *resultList = result->Get<ListInst>();
	// Copy entries to the output
	if (totalLength > 0)
	{
		CopyMemoryT(resultList->values, la->values, la->length);
		CopyMemoryT(resultList->values + la->length, lb->values, lb->length);
	}

	resultList->length = totalLength;
	resultList->version++;

	VM_Push(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_slice1)
{
	// slice(startIndex)
	PinnedAlias<ListInst> list(THISP);

	// Get the start index
	size_t startIndex;
	CHECKED(GetIndex(thread, *list, args + 1, true, startIndex));

	// Create the output list
	Value *output = VM_Local(thread, 0);
	CHECKED(SliceList(thread, *list, startIndex, list->length, output));

	// Return the new list
	VM_Push(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_slice2)
{
	// slice(startIndex, count)
	Aves *aves = Aves::Get(thread);
	PinnedAlias<ListInst> list(THISP);

	// Get the indexes
	size_t startIndex;
	CHECKED(GetIndex(thread, *list, args + 1, true, startIndex));
	CHECKED(IntFromValue(thread, args + 2));
	if (args[2].v.integer < 0)
	{
		VM_PushString(thread, strings::count);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}
	if (args[2].v.integer > OVUM_ISIZE_MAX || startIndex + (size_t)args[2].v.integer > list->length)
	{
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 0);
	}
	size_t count = (size_t)args[2].v.integer;
	size_t endIndex = startIndex + count;

	Value *output = VM_Local(thread, 0);

	CHECKED(SliceList(thread, *list, startIndex, endIndex, output));

	// Return the new list
	VM_Push(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_List_sliceTo)
{
	// sliceTo(startIndex, endIndex)
	// startIndex is inclusive, endIndex is exclusive
	PinnedAlias<ListInst> list(THISP);

	// Get the indexes
	size_t startIndex, endIndex;
	CHECKED(GetIndex(thread, *list, args + 1, true, startIndex));
	CHECKED(GetIndex(thread, *list, args + 2, true, endIndex));

	Value *output = VM_Local(thread, 0);

	CHECKED(SliceList(thread, *list, startIndex, endIndex, output));

	// Return the new list
	VM_Push(thread, output);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_List_reverse)
{
	// Reverse in-place
	ListInst *list = THISV.v.list;
	ReverseArray(list->length, list->values);
	list->version++;
	RETURN_SUCCESS;
}

AVES_API int OVUM_CDECL InitListInstance(ThreadHandle thread, ListInst *list, size_t capacity)
{
	list->capacity = capacity;
	list->length = 0;
	list->version = 0;
	if (capacity == 0)
		list->values = nullptr;
	else
	{
		PinnedAlias<ListInst> p(list);

		int r = GC_AllocValueArray(thread, capacity, &list->values);
		if (r != OVUM_SUCCESS) return r;
	}
	RETURN_SUCCESS;
}

int EnsureMinCapacity(ThreadHandle thread, ListInst *list, size_t capacity)
{
	if (list->capacity < capacity)
		// Try to double the capacity, but make sure we can always satisfy
		// the requested minimum capacity.
		return SetListCapacity(thread, list, max(list->capacity * 2, capacity));
	RETURN_SUCCESS;
}

int SetListCapacity(ThreadHandle thread, ListInst *list, size_t capacity)
{
	Aves *aves = Aves::Get(thread);

	if (capacity < list->length)
	{
		VM_PushString(thread, strings::capacity);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
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

int SliceList(ThreadHandle thread, ListInst *list, size_t startIndex, size_t endIndex, Value *output)
{
	Aves *aves = Aves::Get(thread);

	int status__;

	if (endIndex < startIndex)
	{
		VM_PushNull(thread); // paramName
		VM_PushString(thread, error_strings::EndIndexLessThanStart); // message
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 2);
	}

	size_t sliceLength = endIndex - startIndex;

	// Create the output list
	VM_PushInt(thread, sliceLength); // capacity
	CHECKED(GC_Construct(thread, GetType_List(thread), 1, output));

	if (sliceLength > 0)
	{
		ListInst *outputList = output->v.list;
		OVUM_ASSERT(outputList->capacity >= sliceLength);

		// Copy the elements across
		CopyMemoryT(outputList->values, list->values + startIndex, sliceLength);
		outputList->length = sliceLength;
	}

	// Return the new list
	VM_Push(thread, output);

	status__ = OVUM_SUCCESS;
retStatus__:
	return status__;
}
