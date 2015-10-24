#ifndef AVES__MODULE_H
#define AVES__MODULE_H

#include "../../aves.h"

class ModuleInst
{
public:
	ModuleHandle module;
	String *fileName;
	Value version;
};

AVES_API int OVUM_CDECL aves_reflection_Module_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_new);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_handle);
AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_name);
AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_version);
AVES_API NATIVE_FUNCTION(aves_reflection_Module_get_fileName);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_getType);
AVES_API NATIVE_FUNCTION(aves_reflection_Module_getTypes);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_getFunction);
AVES_API NATIVE_FUNCTION(aves_reflection_Module_getFunctions);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_getGlobalConstant);
AVES_API NATIVE_FUNCTION(aves_reflection_Module_getGlobalConstants);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_getMember);
AVES_API NATIVE_FUNCTION(aves_reflection_Module_getMembers);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_getCurrentModule);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_getSearchDirectories);

AVES_API NATIVE_FUNCTION(aves_reflection_Module_find);

#endif // AVES__MODULE_H
