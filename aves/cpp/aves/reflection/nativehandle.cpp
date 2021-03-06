#include "nativehandle.h"
#include "../../aves_state.h"

using namespace aves;

AVES_API NATIVE_FUNCTION(aves_reflection_NativeHandle_opEquals)
{
	Aves *aves = Aves::Get(thread);

	if (args[1].type == aves->aves.reflection.NativeHandle)
		VM_PushBool(thread, args[0].v.instance == args[1].v.instance);
	else
		VM_PushBool(thread, false);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_reflection_NativeHandle_opPlus)
{
	// Note: read the 'instance' field rather than 'integer',
	// since sizeof(void*) may not be sizeof(int64_t) and the
	// 'integer' field may contain unpredictable padding bytes.
	VM_PushInt(thread, (int64_t)args[0].v.instance);
	RETURN_SUCCESS;
}
