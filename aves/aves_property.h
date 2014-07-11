#pragma once

#ifndef AVES__PROPERTY_H
#define AVES__PROPERTY_H

#include "aves.h"

class PropertyInst
{
public:
	PropertyHandle property;
	String *fullName;
};

AVES_API void CDECL aves_reflection_Property_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_accessLevel);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_handle);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_name);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_f_fullName);
AVES_API NATIVE_FUNCTION(aves_reflection_Property_set_f_fullName);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_fullName);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_declaringType);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_isStatic);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_canRead);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_canWrite);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_getterMethod);

AVES_API NATIVE_FUNCTION(aves_reflection_Property_get_setterMethod);

#endif // AVES__PROPERTY_H