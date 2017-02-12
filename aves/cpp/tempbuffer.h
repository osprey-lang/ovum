#ifndef AVES__TEMPBUFFER_H
#define AVES__TEMPBUFFER_H

#include "aves.h"
#include <memory> // placement new

namespace aves
{
	template<class T, size_t StackSize>
	class TempBuffer
	{
	private:
		T stackItems[StackSize];
		T *heapItems;
		size_t heapSize;

	public:
		inline TempBuffer() :
			heapItems(nullptr),
			heapSize(0)
		{ }

		inline ~TempBuffer()
		{
			if (heapItems != nullptr)
				delete[] heapItems;
		}

		inline T &operator[](size_t index)
		{
			if (heapItems != nullptr)
				return heapItems[index];
			return stackItems[index];
		}

		inline T *GetPointer()
		{
			if (heapItems != nullptr)
				return heapItems;
			return (T*)stackItems;
		}

		inline size_t GetCapacity() const
		{
			if (heapItems != nullptr)
				return heapSize;
			return StackSize;
		}

		inline bool EnsureCapacity(size_t capacity, bool preserveContents = false)
		{
			if (GetCapacity() >= capacity)
				return true;

			T *newItems = new(std::nothrow) T[capacity];
			if (newItems == nullptr)
				return false;

			if (preserveContents)
				CopyMemoryT(newItems, GetPointer(), GetCapacity());

			// Don't leak the old array!
			delete[] heapItems;
			heapItems = newItems;
			heapSize = capacity;

			return true;
		}
	};
}

#endif