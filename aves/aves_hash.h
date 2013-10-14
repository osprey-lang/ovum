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
AVES_API NATIVE_FUNCTION(aves_Hash_removeInternal);

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value);

AVES_API void InitHashInstance(ThreadHandle thread, HashInst *hash, const int32_t capacity);

bool aves_Hash_getReferences(void *basePtr, unsigned int &valc, Value **target);

void aves_Hash_finalize(ThreadHandle thread, void *basePtr);

#endif // AVES__HASH_H