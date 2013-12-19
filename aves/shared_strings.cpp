#include "aves_shared_strings.h"

namespace strings
{
	#define SFS ::StringFlags::STATIC

	LitString<0> _Empty = { 0, 0, SFS, 0 };
	String *Empty = _S(_Empty);

	LitString<6> _format   = { 6, 0, SFS, 'f','o','r','m','a','t',0 };
	LitString<8> _toString = { 8, 0, SFS, 't','o','S','t','r','i','n','g',0 };

	String *format   = _S(_format);
	String *toString = _S(_toString);

	LitString<3> _str      = { 3, 0, SFS, 's','t','r',0 };
	LitString<1> _i        = { 1, 0, SFS, 'i',0 };
	LitString<3> _cur      = { 3, 0, SFS, 'c','u','r',0 };
	LitString<5> _index    = { 5, 0, SFS, 'i','n','d','e','x',0 };
	LitString<8> _capacity = { 8, 0, SFS, 'c','a','p','a','c','i','t','y',0 };
	LitString<6> _values   = { 6, 0, SFS, 'v','a','l','u','e','s',0 };
	LitString<5> _value    = { 5, 0, SFS, 'v','a','l','u','e',0 };
	LitString<5> _times    = { 5, 0, SFS, 't','i','m','e','s',0 };
	LitString<8> _oldValue = { 8, 0, SFS, 'o','l','d','V','a','l','u','e',0 };
	LitString<4> _kind     = { 4, 0, SFS, 'k','i','n','d',0 };
	LitString<3> _key      = { 3, 0, SFS, 'k','e','y',0 };
	LitString<2> _cp       = { 2, 0, SFS, 'c','p',0 };
	LitString<5> __args    = { 5, 0, SFS, '_','a','r','g','s',0 };
	LitString<1> _x        = { 1, 0, SFS, 'x',0 };
	LitString<1> _y        = { 1, 0, SFS, 'y',0 };
	LitString<5> _width    = { 5, 0, SFS, 'w','i','d','t','h',0 };
	LitString<6> _height   = { 5, 0, SFS, 'h','e','i','g','h','t',0 };
	LitString<3> _add      = { 3, 0, SFS, 'a','d','d',0 };

	String *str      = _S(_str);
	String *i        = _S(_i);
	String *cur      = _S(_cur);
	String *index    = _S(_index);
	String *capacity = _S(_capacity);
	String *values   = _S(_values);
	String *value    = _S(_value);
	String *times    = _S(_times);
	String *oldValue = _S(_oldValue);
	String *kind     = _S(_kind);
	String *key      = _S(_key);
	String *cp       = _S(_cp);
	String *_args    = _S(__args);
	String *x        = _S(_x);
	String *y        = _S(_y);
	String *width    = _S(_width);
	String *height   = _S(_height);
	String *add      = _S(_add);

	// TODO: Make this platform-sensitive; \r\n is really just for Windows, you know?
	LitString<2> _newline  = { 2, 0, SFS, '\r','\n',0 };

	String *newline  = _S(_newline);
}

namespace error_strings
{
	LitString<43> _EndIndexLessThanStart = LitString<43>::FromCString("The end index is less than the start index.");
	LitString<44> _HashKeyNotFound = LitString<44>::FromCString("The hash does not contain the specified key.");
	LitString<46> _RadixOutOfRange = LitString<46>::FromCString("The radix must be between 2 and 36, inclusive.");
	LitString<30> _InvalidIntegerFormat = LitString<30>::FromCString("Invalid integer format string.");

	String *EndIndexLessThanStart = _S(_EndIndexLessThanStart);
	String *HashKeyNotFound = _S(_HashKeyNotFound);
	String *RadixOutOfRange = _S(_RadixOutOfRange);
	String *InvalidIntegerFormat = _S(_InvalidIntegerFormat);
}