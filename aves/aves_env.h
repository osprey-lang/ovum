#pragma once

#ifndef AVES__ENV_H
#define AVES__ENV_H

#include "aves.h"

AVES_API NATIVE_FUNCTION(aves_Env_get_args);

AVES_API NATIVE_FUNCTION(aves_Env_get_newline);

AVES_API NATIVE_FUNCTION(aves_Env_get_tickCount);

AVES_API NATIVE_FUNCTION(aves_Env_get_stackTrace);

#endif // AVES__ENV_H