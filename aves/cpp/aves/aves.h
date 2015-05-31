// Contains global functions from the 'aves' namespace,
// as well as general module initialization stuff.

#ifndef AVES__AVES_NS_H
#define AVES__AVES_NS_H

#include "../aves.h"

AVES_API NATIVE_FUNCTION(aves_print);

AVES_API NATIVE_FUNCTION(aves_exit);

// Numeric reinterpretation function things
AVES_API NATIVE_FUNCTION(aves_number_asInt);
AVES_API NATIVE_FUNCTION(aves_number_asUInt);
AVES_API NATIVE_FUNCTION(aves_number_asReal);

#endif // AVES__AVES_NS_H