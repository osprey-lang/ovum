#pragma once

// Contains functions for the Object class, which is the root of the type hierarchy.

#ifndef AVES__OBJECT_H
#define AVES__OBJECT_H

#include "aves.h"

AVES_API NATIVE_FUNCTION(aves_Object_new);

AVES_API NATIVE_FUNCTION(aves_Object_getHashCode);
AVES_API NATIVE_FUNCTION(aves_Object_toString);

AVES_API NATIVE_FUNCTION(aves_Object_opEquals);

#endif // AVES__OBJECT_H