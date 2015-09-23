#ifndef AVES__ARRAY_H
#define AVES__ARRAY_H

#include "../aves.h"

namespace aves
{

class Array
{
public:
	int64_t length;
	Value firstValue;

	int GetIndex(ThreadHandle thread, Value *arg, int64_t &result);
	static size_t GetSize(int64_t length);
};

} // namespace aves

AVES_API int aves_Array_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Array_new);

AVES_API NATIVE_FUNCTION(aves_Array_get_item);
AVES_API NATIVE_FUNCTION(aves_Array_set_item);
AVES_API NATIVE_FUNCTION(aves_Array_get_length);

AVES_API NATIVE_FUNCTION(aves_Array_fillInternal);

AVES_API NATIVE_FUNCTION(aves_Array_copyInternal);

int OVUM_CDECL aves_Array_getReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

#endif // AVES__ARRAY_H