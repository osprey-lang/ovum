#pragma once

#ifndef AVES__METHODBASE_H
#define AVES__METHODBASE_H

#include "../../aves.h"

class MethodBaseInst
{
public:
	MethodHandle method;
	String *cachedName;
};

AVES_API void CDECL aves_reflection_MethodBase_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_new);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_accessLevel);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_handle);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_internalName);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_cachedName);
AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_set_cachedName);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_declaringType);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isGlobal);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isStatic);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isConstructor);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_isImpl);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_get_overloadCount);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_getOverloadHandle);

AVES_API NATIVE_FUNCTION(aves_reflection_MethodBase_invoke);

AVES_API NATIVE_FUNCTION(aves_reflection_Method_get_baseMethod);

#endif // AVES__METHODBASE_H