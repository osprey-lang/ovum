#pragma once

#ifndef AVES__SET_H
#define AVES__SET_H

#include "aves.h"

typedef struct SetEntry_S
{
	int32_t hashCode;
	int32_t next;
	// The value is contained in the 'values' array, at the same index as this entry
} SetEntry;
typedef struct SetInst_S
{
	int32_t capacity;   // the number of "slots" in buckets, entries and values
	int32_t count;      // the number of entries (not buckets) that have been used
	int32_t freeCount;  // the number of entries that were previously used, and have now been freed (and can thus be reused)
	int32_t freeList;   // the index of the first freed entry
	int32_t version;    // the "version" of the hash, incremented whenever changes are made

	int32_t *buckets;   // indexes into entries and values
	SetEntry *entries;  // entries!
	Value *values;      // values! Separate array for GC examination speediness.
} SetInst;

AVES_API void aves_Set_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Set_new);

AVES_API NATIVE_FUNCTION(aves_Set_get_length);

AVES_API NATIVE_FUNCTION(aves_Set_clone);
AVES_API NATIVE_FUNCTION(aves_Set_containsInternal);
AVES_API NATIVE_FUNCTION(aves_Set_addInternal);
AVES_API NATIVE_FUNCTION(aves_Set_removeInternal);
AVES_API NATIVE_FUNCTION(aves_Set_toggleInternal);

AVES_API NATIVE_FUNCTION(aves_Set_get_version);
AVES_API NATIVE_FUNCTION(aves_Set_get_entryCount);
AVES_API NATIVE_FUNCTION(aves_Set_hasEntryAt);
AVES_API NATIVE_FUNCTION(aves_Set_getEntryAt);

bool aves_Set_getReferences(void *basePtr, unsigned int &valc, Value **target);
void aves_Set_finalize(ThreadHandle thread, void *basePtr);

#endif // AVES__SET_H