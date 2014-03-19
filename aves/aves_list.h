#pragma once

#ifndef AVES__LIST_H
#define AVES__LIST_H

#include "aves.h"

AVES_API void aves_List_init(TypeHandle type);

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

AVES_API void InitListInstance(ThreadHandle thread, ListInst *list, const int32_t capacity);

void EnsureMinCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity);

void SetListCapacity(ThreadHandle thread, ListInst *list, const int32_t capacity);

bool aves_List_getReferences(void *basePtr, unsigned int *valc, Value **target, int32_t *state);

void aves_List_finalize(void *basePtr);

#endif // AVES__LIST_H