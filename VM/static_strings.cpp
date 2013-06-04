#include "ov_vm.internal.h"

namespace static_strings
{
	namespace
	{
		LitString<0> _empty = { 0, 0, STR_STATIC, 0 };

		LitString<4> __new  = { 4, 0, STR_STATIC, '.','n','e','w',0 };
		LitString<5> __iter = { 5, 0, STR_STATIC, '.','i','t','e','r',0 };
		LitString<5> __call = { 5, 0, STR_STATIC, '.','c','a','l','l',0 };

		LitString<8> _toString = { 8, 0, STR_STATIC, 't','o','S','t','r','i','n','g',0 };
	}

	String *empty = _S(_empty);

	String *_new  = _S(__new);
	String *_iter = _S(__iter);
	String *_call = _S(__call);

	String *toString = _S(_toString);

	namespace errors
	{
		namespace
		{
			LitString<48> _ToStringWrongType = LitString<48>::FromCString("The return value of toString() must be a string.");
		}

		String *ToStringWrongType = _S(_ToStringWrongType);
	}
}