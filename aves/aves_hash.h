#pragma once

#ifndef AVES__HASH_H
#define AVES__HASH_H

#include "aves.h"

AVES_API void aves_Hash_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Hash_get_length);
AVES_API NATIVE_FUNCTION(aves_Hash_get_capacity);
AVES_API NATIVE_FUNCTION(aves_Hash_get_version);
AVES_API NATIVE_FUNCTION(aves_Hash_get_entryCount);

AVES_API NATIVE_FUNCTION(aves_Hash_initialize);
AVES_API NATIVE_FUNCTION(aves_Hash_getItemInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_getEntry);
AVES_API NATIVE_FUNCTION(aves_Hash_insert);
AVES_API NATIVE_FUNCTION(aves_Hash_hasKeyInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_hasValue);
AVES_API NATIVE_FUNCTION(aves_Hash_tryGetInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_removeInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_pinEntries);
AVES_API NATIVE_FUNCTION(aves_Hash_unpinEntries);

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value);

AVES_API int InitHashInstance(ThreadHandle thread, HashInst *hash, const int32_t capacity);

int aves_Hash_getReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

#endif // AVES__HASH_H