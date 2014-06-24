#pragma once

#ifndef AVES__SET_H
#define AVES__SET_H

#include "aves.h"

typedef struct SetEntry_S
{
	int32_t hashCode;
	int32_t next;
	Value value;
} SetEntry;
typedef struct SetInst_S
{
	int32_t capacity;   // the number of "slots" in buckets, entries and values
	int32_t count;      // the number of entries (not buckets) that have been used
	int32_t freeCount;  // the number of entries that were previously used, and have now been freed (and can thus be reused)
	int32_t freeList;   // the index of the first freed entry
	int32_t version;    // the "version" of the hash, incremented whenever changes are made

	int32_t *buckets;   // indexes into entries
	SetEntry *entries;  // entries!
} SetInst;

AVES_API void CDECL aves_Set_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Set_new);

AVES_API NATIVE_FUNCTION(aves_Set_get_length);

AVES_API NATIVE_FUNCTION(aves_Set_clear);
AVES_API NATIVE_FUNCTION(aves_Set_containsInternal);
AVES_API NATIVE_FUNCTION(aves_Set_addInternal);
AVES_API NATIVE_FUNCTION(aves_Set_removeInternal);

AVES_API NATIVE_FUNCTION(aves_Set_get_version);
AVES_API NATIVE_FUNCTION(aves_Set_get_entryCount);
AVES_API NATIVE_FUNCTION(aves_Set_hasEntryAt);
AVES_API NATIVE_FUNCTION(aves_Set_getEntryAt);

int CDECL aves_Set_getReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

#endif // AVES__SET_H