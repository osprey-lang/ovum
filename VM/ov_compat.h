#pragma once

#ifndef VM__COMPAT_H
#define VM__COMPAT_H

/*
 * This file contains various compatibility and utility macros and methods.
 */

#include <cfloat>
#include <cmath>

// NOTE: if your compiler supports enums with specific underlying types, e.g.
//     enum SOMETHING : long { };
// then #define _TYPED_ENUMS
//
// Microsoft's C++ compiler fully supports this feature, hence _MSC_VER.
//
// If, however, your compiler supports enums with specific underlying types
// of the form
//     enum SOMETHING : class long { };
// then #define _TYPED_CLASS_ENUMS
//
// Otherwise, a fallback will be used.

#if defined(_MSC_VER) || defined(_TYPED_ENUMS)

#define TYPED_ENUM(name,type) enum name : type

#elif define(_TYPED_CLASS_ENUMS)

#define TYPED_ENUM(name,type) enum name : class type

#else

#define TYPED_ENUM(name,type) typedef type name; enum _E_##name

#endif

// For checked multiplication only
// (uses the _mul128 function if available)
#if !defined(__GNUC__) && defined(_M_AMD64)
#include <intrin.h>
#define USE_INTRINSICS 1
#else
#define USE_INTRINSICS 0
#endif

// This is not so much for compatibility as it is for convenience,
// because C++ enums have really dumb semantics.
// It is meant to be used with bitwise enum operations, e.g.:
//    _E(MemberFlags, M_STATIC | M_PUBLIC)
// but because it's just a static_cast, it could be used anywhere
// where a static cast is usable. Not sure I'd recommend that, though.
#define _E(type,expr)  static_cast<type>(expr)


template<class T>
inline T Clamp(const T value, const T max, const T min)
{
	return value < min ? min :
		value > max ? max :
		value;
}
template<int min, int max>
inline int Clamp(const int value)
{
	return value < min ? min :
		value > max ? max :
		value;
}
template<long min, long max>
inline long Clamp(const long value)
{
	return value < min ? min :
		value > max ? max :
		value;
}
template<long long min, long long max>
inline long Clamp(const long long value)
{
	return value < min ? min :
		value > max ? max :
		value;
}

// I'm surprised these are not built into C++.
#ifdef _MSC_VER // Microsoft C++ compiler

#define IsNaN    _isnan
#define IsFinite _finite

#elif

inline const bool IsNaN(const double value) { return value != value; }
inline const bool IsFinite(const double value) { return value <= DBL_MAX && value >= DBL_MIN; }

#endif

template<class T>
inline void ReverseArray(const size_t length, T values[])
{
	T *left  = values;
	T *right = values + length - 1;

	while (left < right)
	{
		T temp = *left;
		*left = *right;
		*right = temp;

		left++;
		right--;
	}
}

// Copies the values from source to destination, in reverse order.
// 'length' is the total number of values to copy.
template<class T>
inline void CopyReversed(T destination[], const T source[], const size_t length)
{
	const T *sourcePointer = source;
	T *destPointer = destination + length - 1;

	while (destPointer >= destination)
	{
		*destPointer = *sourcePointer;
		destPointer--;
		sourcePointer++;
	}
}

// This is really just a paper-thin type-safe wrapper around memcpy.
// The only real difference is that it uses a template.
// The size is multiplied by sizeof(T), by the way, so you don't have
// to worry about that either.
template<class T>
inline void CopyMemoryT(T *destination, const T *source, const size_t size)
{
	memcpy(destination, source, sizeof(T) * size);
}

// Finds the smallest power of two that is greater than or equal to the given number.
inline uint32_t NextPowerOfTwo(uint32_t n)
{
	n--; // When n == 0, this overflows on purpose
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

// Finds the smallest power of two that is greater than or equal to the given number.
inline int32_t NextPowerOfTwo(int32_t n)
{
	n--; // When n == 0, this overflows on purpose
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

#endif // VM__COMPAT_H