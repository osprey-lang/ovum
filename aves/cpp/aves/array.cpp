#include "array.h"
#include "../aves_state.h"
#include <climits>

using namespace aves;

namespace aves
{

int Array::GetIndex(ThreadHandle thread, Value *arg, int64_t &result)
{
	Aves *aves = Aves::Get(thread);

	int r = IntFromValue(thread, arg);
	if (r != OVUM_SUCCESS)
		return r;
	int64_t index = arg->v.integer;
	if (index < 0 || index >= this->length)
	{
		VM_PushString(thread, strings::index);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	result = index;
	RETURN_SUCCESS;
}

size_t Array::GetSize(int64_t length)
{
	if (length < 0)
		return 0;

	// Let's calculate the total number of bytes required for
	// all the items of the array.
	int64_t itemsSize;
	int r = Int_MultiplyChecked(length, sizeof(Value), itemsSize);
	if (r != OVUM_SUCCESS)
		return 0;

	// And now let's add the size of the Array class itself,
	// minus the first item.
	int64_t size;
	r = Int_AddChecked(itemsSize, (int64_t)(sizeof(Array) - sizeof(Value)), size);
	if (r != OVUM_SUCCESS)
		return 0;

#if SIZE_MAX < INT64_MAX
	if (size > (int64_t)SIZE_MAX)
		return 0;
#endif

	return (size_t)size;
}

} // namespace aves

AVES_API int aves_Array_init(TypeHandle type)
{
	Type_SetReferenceWalker(type, aves_Array_walkReferences);
	Type_SetConstructorIsAllocator(type, true);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Array_new)
{
	// new(length)
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));
	int64_t length = args[1].v.integer;

	// Let's calculate the total size of the array we need to allocate.
	// Array::GetSize can take care of this. If it returns 0, length is
	// out of range.
	size_t size = Array::GetSize(length);
	if (size == 0)
	{
		VM_PushString(thread, strings::length);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	// Allocate the array and put it in the 'this' argument, since it's
	// unused anyway, and then initialize it.
	CHECKED(GC_Alloc(thread, aves->aves.Array, size, THISP));
	Array *array = THISV.Get<Array>();
	array->length = length;

	// And that's it! Let's just return the value.
	VM_Push(thread, THISP);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Array_get_item)
{
	Array *array = THISV.Get<Array>();

	int64_t index;
	CHECKED(array->GetIndex(thread, args + 1, index));

	VM_Push(thread, &array->firstValue + index);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Array_set_item)
{
	Array *array = THISV.Get<Array>();

	int64_t index;
	CHECKED(array->GetIndex(thread, args + 1, index));

	(&array->firstValue)[index] = args[2];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Array_get_length)
{
	Array *array = THISV.Get<Array>();
	VM_PushInt(thread, array->length);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Array_fillInternal)
{
	// fillInternal(value, startIndex is Int, count is Int)
	// The external methods range-check the arguments.

	Array *array = THISV.Get<Array>();
	Value *value = args + 1;
	int64_t startIndex = args[2].v.integer;
	int64_t count = args[3].v.integer;

	Value *items = &array->firstValue;
	while (count > 0)
	{
		items[startIndex] = *value;
		startIndex++;
		count--;
	}

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Array_copyInternal)
{
	// copyInternal(source is Array, sourceIndex is Int, dest is Array, destIndex is Int, count is Int)
	// The external methods range-check all the arguments.
	Array *src = args[0].Get<Array>();
	Array *dest = args[2].Get<Array>();
	int64_t srcIndex = args[1].v.integer;
	int64_t destIndex = args[3].v.integer;
	int64_t count = args[4].v.integer;

	// If count fits within the source and destination arrays, it must be small enough for size_t.
	size_t size = (size_t)count * sizeof(Value);
	// Copying this data could take a while, if count is particularly large. However, the array
	// contains managed references, which means we can't enter a native region here: if the GC
	// runs, the destination array must be in a consistent state.
	// Note: use memmove to make it safe for src and dest to overlap.
	memmove(&dest->firstValue + destIndex, &src->firstValue + srcIndex, size);

	RETURN_SUCCESS;
}

int OVUM_CDECL aves_Array_walkReferences(void *basePtr, ReferenceVisitor callback, void *cbState)
{
	Array *array = reinterpret_cast<Array*>(basePtr);

	Value *items = &array->firstValue;
	int64_t length = array->length;

#if OVUM_64BIT
	// Unlikely case - the array has more than 4 billion entries.
	// Still, we have to support that as well.
	while (length > UINT_MAX)
	{
		callback(cbState, UINT_MAX, items);
		items += UINT_MAX;
		length -= UINT_MAX;
	}
#endif

	callback(cbState, (unsigned int)length, items);

	RETURN_SUCCESS;
}
