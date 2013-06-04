#pragma once

// Contains the implementation of aves.Boolean

#ifndef AVES__BOOLEAN_H
#define AVES__BOOLEAN_H

#include "aves.h"

AVES_API NATIVE_FUNCTION(aves_bool);

AVES_API NATIVE_FUNCTION(aves_Boolean_getHashCode);

AVES_API NATIVE_FUNCTION(aves_Boolean_toString);

AVES_API NATIVE_FUNCTION(aves_Boolean_opEquals);

AVES_API NATIVE_FUNCTION(aves_Boolean_opCompare);

AVES_API NATIVE_FUNCTION(aves_Boolean_opPlus);

#endif // AVES_BOOLEAN_H