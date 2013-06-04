#include "aves_shared_strings.h"

namespace strings
{
	LitString<6> _format   = { 6, 0, STR_STATIC, 'f','o','r','m','a','t',0 };
	LitString<8> _toString = { 8, 0, STR_STATIC, 't','o','S','t','r','i','n','g',0 };

	String *format   = _S(_format);
	String *toString = _S(_toString);

	LitString<3> _str      = { 3, 0, STR_STATIC, 's','t','r',0 };
	LitString<1> _i        = { 1, 0, STR_STATIC, 'i',0 };
	LitString<3> _cur      = { 3, 0, STR_STATIC, 'c','u','r',0 };
	LitString<5> _index    = { 5, 0, STR_STATIC, 'i','n','d','e','x',0 };
	LitString<8> _capacity = { 8, 0, STR_STATIC, 'c','a','p','a','c','i','t','y',0 };

	String *str      = _S(_str);
	String *i        = _S(_i);
	String *cur      = _S(_cur);
	String *index    = _S(_index);
	String *capacity = _S(_capacity);
}

namespace error_strings
{
	LitString<43> _EndIndexLessThanStart = LitString<43>::FromCString("The end index is less than the start index.");

	String *EndIndexLessThanStart = _S(_EndIndexLessThanStart);
}