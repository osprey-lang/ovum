#include "ov_string.h"
#include "ov_stringbuffer.h"
#include "aves_error.h"

#define _E(value)	((value).common.error)

LitString<30> _DefaultErrorMessage = LitString<30>::FromCString("An unspecified error occurred.");
String *DefaultErrorMessage = _S(_DefaultErrorMessage);

AVES_API void aves_Error_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ErrorInst));
	Type_SetReferenceGetter(type, aves_Error_getReferences);
}

AVES_API NATIVE_FUNCTION(aves_Error_new)
{
	// Arguments:
	//     ()
	//     (message)
	//     (message, innerError)
	// Remember: argc includes the instance.

	ErrorInst *err = _E(THISV);

	if (argc > 1 && !IS_NULL(args[1]))
	{
		StringFromValue(thread, args + 1);
		err->message = args[1].common.string;
	}
	else
		err->message = DefaultErrorMessage;

	if (argc > 2)
		err->innerError = args[2];
}

AVES_API NATIVE_FUNCTION(aves_Error_get_message)
{
	ErrorInst *err = _E(THISV);
	VM_PushString(thread, err->message);
}
AVES_API NATIVE_FUNCTION(aves_Error_get_stackTrace)
{
	ErrorInst *err = _E(THISV);
	if (err->stackTrace == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, err->stackTrace);
}
AVES_API NATIVE_FUNCTION(aves_Error_get_innerError)
{
	ErrorInst *err = _E(THISV);
	VM_Push(thread, err->innerError);
}
AVES_API NATIVE_FUNCTION(aves_Error_get_data)
{
	ErrorInst *err = _E(THISV);
	VM_Push(thread, err->data);
}

bool aves_Error_getReferences(void *basePtr, unsigned int *valc, Value **target, int32_t *state)
{
	ErrorInst *err = reinterpret_cast<ErrorInst*>(basePtr);

	*valc = 2;
	*target = &err->innerError;

	return false;
}