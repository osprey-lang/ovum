#include "gc.h"

AVES_API NATIVE_FUNCTION(aves_GC_get_collectCount)
{
	VM_PushInt(thread, GC_GetCollectCount(thread));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_GC_collect)
{
	GC_Collect(thread);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_GC_getGeneration)
{
	VM_PushInt(thread, GC_GetGeneration(args));
	RETURN_SUCCESS;
}
