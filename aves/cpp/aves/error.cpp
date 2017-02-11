#include "error.h"
#include <ovum_string.h>
#include <ovum_stringbuffer.h>

LitString<30> _DefaultErrorMessage = LitString<30>::FromCString("An unspecified error occurred.");
String *DefaultErrorMessage = _DefaultErrorMessage.AsString();

AVES_API int OVUM_CDECL aves_Error_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(ErrorInst));

	int status__;
	CHECKED(Type_AddNativeField(type, offsetof(ErrorInst, message),    NativeFieldType::STRING));
	CHECKED(Type_AddNativeField(type, offsetof(ErrorInst, stackTrace), NativeFieldType::STRING));
	CHECKED(Type_AddNativeField(type, offsetof(ErrorInst, innerError), NativeFieldType::VALUE));
	CHECKED(Type_AddNativeField(type, offsetof(ErrorInst, data),       NativeFieldType::VALUE));

retStatus__:
	return status__;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Error_new)
{
	// Arguments:
	//     ()
	//     (message)
	//     (message, innerError)
	// Remember: argc includes the instance.

	Alias<ErrorInst> err(THISP);

	if (argc > 1 && !IS_NULL(args[1]))
	{
		CHECKED(StringFromValue(thread, args + 1));
		err->message = args[1].v.string;
	}
	else
		err->message = DefaultErrorMessage;

	if (argc > 2)
		err->innerError = args[2];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Error_get_message)
{
	ErrorInst *err = THISV.v.error;
	VM_PushString(thread, err->message);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Error_get_stackTrace)
{
	ErrorInst *err = THISV.v.error;
	if (err->stackTrace == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, err->stackTrace);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Error_get_innerError)
{
	ErrorInst *err = THISV.v.error;
	VM_Push(thread, &err->innerError);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Error_get_data)
{
	ErrorInst *err = THISV.v.error;
	VM_Push(thread, &err->data);
	RETURN_SUCCESS;
}
