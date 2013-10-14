#pragma once

#ifndef VM__HELPERS_H
#define VM__HELPERS_H

// Various helper functions

#include "ov_vm.h"

OVUM_API Value IntFromValue(ThreadHandle thread, Value v);

OVUM_API Value UIntFromValue(ThreadHandle thread, Value v);

OVUM_API Value RealFromValue(ThreadHandle thread, Value v);

OVUM_API Value StringFromValue(ThreadHandle thread, Value v);


// Checked arithmetics

// INT
OVUM_API int64_t Int_AddChecked(ThreadHandle thread, const int64_t left, const int64_t right);
OVUM_API int64_t Int_SubtractChecked(ThreadHandle thread, const int64_t left, const int64_t right);
OVUM_API int64_t Int_MultiplyChecked(ThreadHandle thread, const int64_t left, const int64_t right);
OVUM_API int64_t Int_DivideChecked(ThreadHandle thread, const int64_t left, const int64_t right);
OVUM_API int64_t Int_ModuloChecked(ThreadHandle thread, const int64_t left, const int64_t right);

// UINT
OVUM_API uint64_t UInt_AddChecked(ThreadHandle thread, const uint64_t left, const uint64_t right);
OVUM_API uint64_t UInt_SubtractChecked(ThreadHandle thread, const uint64_t left, const uint64_t right);
OVUM_API uint64_t UInt_MultiplyChecked(ThreadHandle thread, const uint64_t left, const uint64_t right);
OVUM_API uint64_t UInt_DivideChecked(ThreadHandle thread, const uint64_t left, const uint64_t right);
OVUM_API uint64_t UInt_ModuloChecked(ThreadHandle thread, const uint64_t left, const uint64_t right);

// HASH HELPERS

// Gets the next prime number greater than or equal to the given value.
// The prime number is suitable for use as the size of a hash table.
OVUM_API int32_t HashHelper_GetPrime(const int32_t min);


#endif // VM__HELPERS_H