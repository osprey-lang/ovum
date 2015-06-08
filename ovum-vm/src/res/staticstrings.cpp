#include "../vm.h"

namespace ovum
{

namespace static_strings
{
	namespace
	{
		LitString<0> _empty = { 0, 0, StringFlags::STATIC, 0 };

		LitString<4> __new  = { 4, 0, StringFlags::STATIC, '.','n','e','w',0 };
		LitString<5> __iter = { 5, 0, StringFlags::STATIC, '.','i','t','e','r',0 };
		LitString<5> __call = { 5, 0, StringFlags::STATIC, '.','c','a','l','l',0 };
		LitString<5> __init = { 5, 0, StringFlags::STATIC, '.','i','n','i','t',0 };
		LitString<5> __item = { 5, 0, StringFlags::STATIC, '.','i','t','e','m',0 };

		LitString<7> _message = { 7, 0, StringFlags::STATIC, 'm','e','s','s','a','g','e',0 };
		LitString<8> _toString = { 8, 0, StringFlags::STATIC, 't','o','S','t','r','i','n','g',0 };
	}

	String *empty = _empty.AsString();

	String *_new  = __new.AsString();
	String *_iter = __iter.AsString();
	String *_call = __call.AsString();
	String *_init = __init.AsString();
	String *_item = __item.AsString();

	String *message = _message.AsString();
	String *toString = _toString.AsString();

	namespace errors
	{
		LitString<48> _ToStringWrongType = LitString<48>::FromCString("The return value of toString() must be a string.");

		String *ToStringWrongType = _ToStringWrongType.AsString();
	}
}

} // namespace ovum