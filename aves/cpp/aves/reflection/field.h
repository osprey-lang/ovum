#ifndef AVES__FIELD_H
#define AVES__FIELD_H

#include "../../aves.h"

class FieldInst
{
public:
	FieldHandle field;
	String *fullName; // cached value
};

AVES_API void CDECL aves_reflection_Field_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_new);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_accessLevel);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_handle);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_name);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_f_fullName);
AVES_API NATIVE_FUNCTION(aves_reflection_Field_set_f_fullName);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_declaringType);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_get_isStatic);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_getValue);

AVES_API NATIVE_FUNCTION(aves_reflection_Field_setValue);

#endif // AVES__FIELD_H