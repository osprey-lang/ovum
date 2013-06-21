#include "aves_shared_strings.h"

namespace strings
{
	LitString<0> _Empty = { 0, 0, STR_STATIC, 0 };
	String *Empty = _S(_Empty);

	LitString<6> _format   = { 6, 0, STR_STATIC, 'f','o','r','m','a','t',0 };
	LitString<8> _toString = { 8, 0, STR_STATIC, 't','o','S','t','r','i','n','g',0 };

	String *format   = _S(_format);
	String *toString = _S(_toString);

	LitString<3> _str      = { 3, 0, STR_STATIC, 's','t','r',0 };
	LitString<1> _i        = { 1, 0, STR_STATIC, 'i',0 };
	LitString<3> _cur      = { 3, 0, STR_STATIC, 'c','u','r',0 };
	LitString<5> _index    = { 5, 0, STR_STATIC, 'i','n','d','e','x',0 };
	LitString<8> _capacity = { 8, 0, STR_STATIC, 'c','a','p','a','c','i','t','y',0 };
	LitString<6> _values   = { 6, 0, STR_STATIC, 'v','a','l','u','e','s',0 };
	LitString<5> _times    = { 5, 0, STR_STATIC, 't','i','m','e','s',0 };
	LitString<8> _oldValue = { 8, 0, STR_STATIC, 'o','l','d','V','a','l','u','e',0 };
	LitString<4> _kind     = { 4, 0, STR_STATIC, 'k','i','n','d',0 };
	LitString<5> __args    = { 5, 0, STR_STATIC, '_','a','r','g','s',0 };

	String *str      = _S(_str);
	String *i        = _S(_i);
	String *cur      = _S(_cur);
	String *index    = _S(_index);
	String *capacity = _S(_capacity);
	String *values   = _S(_values);
	String *times    = _S(_times);
	String *oldValue = _S(_oldValue);
	String *kind     = _S(_kind);
	String *_args    = _S(__args);

	// TODO: Make this platform-sensitive; \r\n is really just for Windows, you know?
	LitString<2> _newline  = { 2, 0, STR_STATIC, '\r','\n',0 };

	String *newline  = _S(newline);
}

namespace error_strings
{
	LitString<43> _EndIndexLessThanStart = LitString<43>::FromCString("The end index is less than the start index.");

	String *EndIndexLessThanStart = _S(_EndIndexLessThanStart);
}