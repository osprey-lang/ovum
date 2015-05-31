#ifndef AVES__GC_H
#define AVES__GC_H

#include "../aves.h"

AVES_API NATIVE_FUNCTION(aves_GC_get_collectCount);

AVES_API NATIVE_FUNCTION(aves_GC_collect);

AVES_API NATIVE_FUNCTION(aves_GC_getGeneration);

#endif // AVES__GC_H