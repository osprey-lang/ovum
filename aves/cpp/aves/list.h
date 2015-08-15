#ifndef AVES__LIST_H
#define AVES__LIST_H

#include "../aves.h"

AVES_API void OVUM_CDECL aves_List_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_List_new);
AVES_API NATIVE_FUNCTION(aves_List_newCap);

AVES_API NATIVE_FUNCTION(aves_List_get_length);

AVES_API NATIVE_FUNCTION(aves_List_get_capacity);
AVES_API NATIVE_FUNCTION(aves_List_set_capacity);

AVES_API NATIVE_FUNCTION(aves_List_get_version);

AVES_API NATIVE_FUNCTION(aves_List_get_item);
AVES_API NATIVE_FUNCTION(aves_List_set_item);

AVES_API NATIVE_FUNCTION(aves_List_add);
AVES_API NATIVE_FUNCTION(aves_List_insert);
AVES_API NATIVE_FUNCTION(aves_List_removeAt);
AVES_API NATIVE_FUNCTION(aves_List_clear);
AVES_API NATIVE_FUNCTION(aves_List_slice1);
AVES_API NATIVE_FUNCTION(aves_List_slice2);
AVES_API NATIVE_FUNCTION(aves_List_reverse);

AVES_API int OVUM_CDECL InitListInstance(ThreadHandle thread, ListInst *list, const int32_t capacity);

AVES_API int OVUM_CDECL ConcatenateLists(ThreadHandle thread, Value *a, Value *b, Value *result);

int EnsureMinCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity);

int SetListCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity);

int SliceList(ThreadHandle thread, ListInst *list, int32_t startIndex, int32_t endIndex, Value *output);

#endif // AVES__LIST_H
