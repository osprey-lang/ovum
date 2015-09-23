#ifndef AVES__TYPE_H
#define AVES__TYPE_H

#include "../../aves.h"

typedef struct TypeInst_S
{
	TypeHandle type;
	String *name; // cached value
} TypeInst;

AVES_API int OVUM_CDECL aves_reflection_Type_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_handle);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_f_name);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_set_f_name);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_fullName);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_declaringModule);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_baseType);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isPrivate);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isAbstract);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isInheritable);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isStatic);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_isPrimitive);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_get_canIterate);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_createInstance);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_inheritsFromInternal);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_isInstance);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_getField);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_getFields);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_getMethod);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_getMethods);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_getProperty);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_getProperties);

AVES_API NATIVE_FUNCTION(aves_reflection_Type_getMember);
AVES_API NATIVE_FUNCTION(aves_reflection_Type_getMembers);

AVES_API int OVUM_CDECL InitTypeToken(ThreadHandle thread, void *basePtr, TypeHandle type);

// These values must be synchronised with those in aves/reflection/Type.osp
enum class MemberSearchFlags : int32_t
{
	NONE          = 0,
	PUBLIC        = 0x01,
	NON_PUBLIC    = 0x02,
	INSTANCE      = 0x04,
	STATIC        = 0x08,
	DECLARED_ONLY = 0x10,

	ACCESSIBILITY = PUBLIC | NON_PUBLIC,
	INSTANCENESS  = INSTANCE | STATIC,
};
OVUM_ENUM_OPS(MemberSearchFlags, int32_t);

#endif // AVES__TYPE_H
