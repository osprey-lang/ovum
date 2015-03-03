#pragma once

#ifndef VM__COMPAT_H
#define VM__COMPAT_H

/*
 * This file contains various compatibility and utility macros and methods.
 */

#define ENUM_OPS(TEnum,TInt) \
	inline TEnum operator&(const TEnum a, const TEnum b) { return static_cast<TEnum>((TInt)a & (TInt)b); } \
	inline TEnum operator|(const TEnum a, const TEnum b) { return static_cast<TEnum>((TInt)a | (TInt)b); } \
	inline TEnum operator^(const TEnum a, const TEnum b) { return static_cast<TEnum>((TInt)a ^ (TInt)b); } \
	inline TEnum &operator&=(TEnum &a, const TEnum b) { a = a & b; return a; } \
	inline TEnum &operator|=(TEnum &a, const TEnum b) { a = a | b; return a; } \
	inline TEnum &operator^=(TEnum &a, const TEnum b) { a = a ^ b; return a; } \
	inline TEnum operator~(const TEnum a) { return static_cast<TEnum>(~(TInt)a); }

// For checked multiplication only
// (uses the _mul128 function if available)
#if !defined(__GNUC__) && defined(_M_AMD64)
# include <intrin.h>
# define USE_INTRINSICS 1
#else
# define USE_INTRINSICS 0
#endif


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
inline long long Clamp(const long long value)
{
	return value < min ? min :
		value > max ? max :
		value;
}


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
template<class T>
inline T NextPowerOfTwo(T n)
{
	n--; // When n == 0, this overflows on purpose
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

// This macro is effectively equivalent to ceil(size / alignment) * alignment,
// but for integer types, and if size and alignment are both constant values, it
// can be fully evaluated at compile-time.
#define ALIGN_TO(size,alignment)  ((size + (alignment) - 1) / (alignment) * (alignment))

// Marks a function member as having been deleted, if the compiler supports it.
// TODO: Better conditional definition based on compiler capabilities.
#if defined(_MSC_VER) && _MSC_VER > 1800
#define OVUM_DELETE =delete
#else
#define OVUM_DELETE
#endif

// When put in the 'private' section of a type definition, disables copying and
// assignment, by declaring operator= and the T(const T&) constructor as private
// members. This should be used on any type that must not be copied by value.
#define DISABLE_COPY_AND_ASSIGN(TypeName)  \
	TypeName(const TypeName&) OVUM_DELETE; \
	void operator=(const TypeName&) OVUM_DELETE

// Disallows parameterless construction, in addition to disabling the copy constructor
// and operator=. This must be put in the 'private' section of a type definition.
#define DISABLE_IMPLICIT_CONSTRUCTION(TypeName) \
	TypeName() OVUM_DELETE;                     \
	DISABLE_COPY_AND_ASSIGN(TypeName)

#ifndef CDECL
# if defined(_MSC_VER)
#  define CDECL __cdecl
# elif defined(__GNUC__)
#  define CDECL __attribute__((cdecl))
# else
// Disable the feature
#  define CDECL
# endif
#endif

#ifndef NOINLINE
# if defined(_MSC_VER)
#  define NOINLINE __declspec(noinline)
# elif defined(__GNUC__)
#  define NOINLINE __attribute__((noinline))
# else
// Disable the feature
#  define NOINLINE
# endif
#endif

#endif // VM__COMPAT_H