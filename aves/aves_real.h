#pragma once

#ifndef AVES__REAL_H
#define AVES__REAL_H

#include "aves.h"

AVES_API NATIVE_FUNCTION(aves_real);

AVES_API NATIVE_FUNCTION(aves_Real_get_isNaN);
AVES_API NATIVE_FUNCTION(aves_Real_get_isInfinite);

AVES_API NATIVE_FUNCTION(aves_Real_getHashCode);
AVES_API NATIVE_FUNCTION(aves_Real_toString);

AVES_API NATIVE_FUNCTION(aves_Real_parseInternal);

AVES_API NATIVE_FUNCTION(aves_Real_opEquals);
AVES_API NATIVE_FUNCTION(aves_Real_opCompare);
AVES_API NATIVE_FUNCTION(aves_Real_opAdd);
AVES_API NATIVE_FUNCTION(aves_Real_opSubtract);
AVES_API NATIVE_FUNCTION(aves_Real_opMultiply);
AVES_API NATIVE_FUNCTION(aves_Real_opDivide);
AVES_API NATIVE_FUNCTION(aves_Real_opModulo);
AVES_API NATIVE_FUNCTION(aves_Real_opPower);
AVES_API NATIVE_FUNCTION(aves_Real_opPlus);
AVES_API NATIVE_FUNCTION(aves_Real_opNegate);

namespace real
{
	int Compare(double left, double right);
}

#endif // AVES__REAL_H