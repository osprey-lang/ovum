#pragma once

// Contains global functions from the 'aves' namespace,
// as well as general module initialization stuff.

#ifndef AVES__AVES_NS_H
#define AVES__AVES_NS_H

#include "aves.h"

extern TypeHandle ArgumentError;
extern TypeHandle ArgumentNullError;
extern TypeHandle ArgumentRangeError;
extern TypeHandle UnicodeCategoryType;
extern TypeHandle BufferViewKindType;

AVES_API void aves_init(ModuleHandle module);

AVES_API NATIVE_FUNCTION(aves_print);
AVES_API NATIVE_FUNCTION(aves_printf);

AVES_API NATIVE_FUNCTION(aves_exit);

#endif // AVES__AVES_NS_H