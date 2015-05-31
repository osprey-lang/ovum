#pragma once

#ifndef AVES__ENUM_H
#define AVES__ENUM_H

#include "../aves.h"

AVES_API NATIVE_FUNCTION(aves_Enum_getHashCode);
AVES_API NATIVE_FUNCTION(aves_Enum_toString);

AVES_API NATIVE_FUNCTION(aves_Enum_opEquals);
AVES_API NATIVE_FUNCTION(aves_Enum_opCompare);
AVES_API NATIVE_FUNCTION(aves_Enum_opPlus);

AVES_API NATIVE_FUNCTION(aves_EnumSet_hasFlag);

AVES_API NATIVE_FUNCTION(aves_EnumSet_opOr);
AVES_API NATIVE_FUNCTION(aves_EnumSet_opXor);
AVES_API NATIVE_FUNCTION(aves_EnumSet_opAnd);
AVES_API NATIVE_FUNCTION(aves_EnumSet_opNot);

#endif // AVES__ENUM_H