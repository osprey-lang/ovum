#pragma once

#ifndef AVES__OVERLOAD_H
#define AVES__OVERLOAD_H

#include "../../aves.h"

class OverloadInst
{
public:
	OverloadHandle overload;
	int32_t index;
	Value method;
};

AVES_API void CDECL aves_reflection_Overload_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_new);

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_handle);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_method);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_index);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isConstructor);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isOverridable);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isAbstract);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isVariadic);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_isNative);
AVES_API NATIVE_FUNCTION(aves_reflection_Overload_get_paramCount);

AVES_API NATIVE_FUNCTION(aves_reflection_Overload_getCurrentOverload);

class ParamInst
{
public:
	ParamInfo param;
	int32_t index;
	Value overload;
};

AVES_API void CDECL aves_reflection_Parameter_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_new);

AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_overload);
AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_index);
AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_name);
AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isByRef);
AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isOptional);
AVES_API NATIVE_FUNCTION(aves_reflection_Parameter_get_isVariadic);

#endif // AVES__OVERLOAD_H