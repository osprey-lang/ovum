#pragma once

#ifndef AVES__ENV_H
#define AVES__ENV_H

#include "aves.h"

AVES_API void aves_Env_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Env_snew);

AVES_API NATIVE_FUNCTION(aves_Env_get_newline);

AVES_API NATIVE_FUNCTION(aves_Env_get_tickCount);

#endif // AVES__ENV_H