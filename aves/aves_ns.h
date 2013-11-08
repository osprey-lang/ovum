#pragma once

// Contains global functions from the 'aves' namespace,
// as well as general module initialization stuff.

#ifndef AVES__AVES_NS_H
#define AVES__AVES_NS_H

#include "aves.h"

class Types
{
public:
	static TypeHandle Int;
	static TypeHandle UInt;
	static TypeHandle Real;
	static TypeHandle ArgumentError;
	static TypeHandle ArgumentNullError;
	static TypeHandle ArgumentRangeError;
	static TypeHandle DuplicateKeyError;
	static TypeHandle UnicodeCategory;
	static TypeHandle BufferViewKind;
	static TypeHandle HashEntry;
	static TypeHandle ConsoleColor;
	static TypeHandle ConsoleKey;
	static TypeHandle ConsoleKeyCode;
};

AVES_API NATIVE_FUNCTION(aves_print);

AVES_API NATIVE_FUNCTION(aves_exit);

// Numeric reinterpretation function things
AVES_API NATIVE_FUNCTION(aves_number_asInt);
AVES_API NATIVE_FUNCTION(aves_number_asUInt);
AVES_API NATIVE_FUNCTION(aves_number_asReal);

#endif // AVES__AVES_NS_H