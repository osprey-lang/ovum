#include "shared_strings.h"

namespace strings
{
	#define SFS ::StringFlags::STATIC

	LitString<0> _Empty = { 0, 0, SFS, 0 };
	String *Empty = _Empty.AsString();

	LitString<6> _format   = { 6, 0, SFS, 'f','o','r','m','a','t',0 };
	LitString<8> _toString = { 8, 0, SFS, 't','o','S','t','r','i','n','g',0 };

	String *format   = _format.AsString();
	String *toString = _toString.AsString();

	LitString<3> _str       = { 3, 0, SFS, 's','t','r',0 };
	LitString<1> _i         = { 1, 0, SFS, 'i',0 };
	LitString<3> _cur       = { 3, 0, SFS, 'c','u','r',0 };
	LitString<5> _index     = { 5, 0, SFS, 'i','n','d','e','x',0 };
	LitString<8> _capacity  = { 8, 0, SFS, 'c','a','p','a','c','i','t','y',0 };
	LitString<6> _values    = { 6, 0, SFS, 'v','a','l','u','e','s',0 };
	LitString<5> _value     = { 5, 0, SFS, 'v','a','l','u','e',0 };
	LitString<5> _times     = { 5, 0, SFS, 't','i','m','e','s',0 };
	LitString<8> _oldValue  = { 8, 0, SFS, 'o','l','d','V','a','l','u','e',0 };
	LitString<4> _kind      = { 4, 0, SFS, 'k','i','n','d',0 };
	LitString<3> _key       = { 3, 0, SFS, 'k','e','y',0 };
	LitString<2> _cp        = { 2, 0, SFS, 'c','p',0 };
	LitString<5> __args     = { 5, 0, SFS, '_','a','r','g','s',0 };
	LitString<1> _x         = { 1, 0, SFS, 'x',0 };
	LitString<1> _y         = { 1, 0, SFS, 'y',0 };
	LitString<5> _width     = { 5, 0, SFS, 'w','i','d','t','h',0 };
	LitString<6> _height    = { 5, 0, SFS, 'h','e','i','g','h','t',0 };
	LitString<3> _add       = { 3, 0, SFS, 'a','d','d',0 };
	LitString<4> _path      = { 4, 0, SFS, 'p','a','t','h',0 };
	LitString<4> _mode      = { 4, 0, SFS, 'm','o','d','e',0 };
	LitString<6> _access    = { 6, 0, SFS, 'a','c','c','e','s','s',0 };
	LitString<5> _share     = { 5, 0, SFS, 's','h','a','r','e',0 };
	LitString<6> _origin    = { 6, 0, SFS, 'o','r','i','g','i','n',0 };
	LitString<6> _handle    = { 6, 0, SFS, 'h','a','n','d','l','e',0 };
	LitString<5> _flags     = { 5, 0, SFS, 'f','l','a','g','s',0 };
	LitString<8> _overload  = { 8, 0, SFS, 'o','v','e','r','l','o','a','d',0 };
	LitString<5> _major     = { 5, 0, SFS, 'm','a','j','o','r',0 };
	LitString<5> _minor     = { 5, 0, SFS, 'm','i','n','o','r',0 };
	LitString<5> _build     = { 5, 0, SFS, 'b','u','i','l','d',0 };
	LitString<8> _revision  = { 8, 0, SFS, 'r','e','v','i','s','i','o','n',0 };
	LitString<9> _minLength = { 9, 0, SFS, 'm','i','n','L','e','n','g','t','h',0 };
	LitString<4> _side      = { 4, 0, SFS, 's','i','d','e',0 };
	LitString<4> _size      = { 4, 0, SFS, 's','i','z','e',0 };
	LitString<5> __call     = { 5, 0, SFS, '.','c','a','l','l',0 };
	LitString<5> __iter     = { 5, 0, SFS, '.','i','t','e','r',0 };
	LitString<4> __new      = { 4, 0, SFS, '.','n','e','w',0 };
	LitString<6> _equals    = { 6, 0, SFS, 'e','q','u','a','l','s',0 };

	String *str       = _str.AsString();
	String *i         = _i.AsString();
	String *cur       = _cur.AsString();
	String *index     = _index.AsString();
	String *capacity  = _capacity.AsString();
	String *values    = _values.AsString();
	String *value     = _value.AsString();
	String *times     = _times.AsString();
	String *oldValue  = _oldValue.AsString();
	String *kind      = _kind.AsString();
	String *key       = _key.AsString();
	String *cp        = _cp.AsString();
	String *_args     = __args.AsString();
	String *x         = _x.AsString();
	String *y         = _y.AsString();
	String *width     = _width.AsString();
	String *height    = _height.AsString();
	String *add       = _add.AsString();
	String *path      = _path.AsString();
	String *mode      = _mode.AsString();
	String *access    = _access.AsString();
	String *share     = _share.AsString();
	String *origin    = _origin.AsString();
	String *handle    = _handle.AsString();
	String *flags     = _flags.AsString();
	String *overload  = _overload.AsString();
	String *major     = _major.AsString();
	String *minor     = _minor.AsString();
	String *build     = _build.AsString();
	String *revision  = _revision.AsString();
	String *minLength = _minLength.AsString();
	String *side      = _side.AsString();
	String *size      = _size.AsString();
	String *_call     = __call.AsString();
	String *_iter     = __iter.AsString();
	String *_new      = __new.AsString();
	String *equals    = _equals.AsString();

#if OVUM_WINDOWS
	LitString<2> _newline = { 2, 0, SFS, '\r','\n',0 };
#else
	LitString<1> _newline = { 1, 0, SFS, '\n',0 };
#endif

	String *newline  = _newline.AsString();
}

namespace error_strings
{
	LitString<43> _EndIndexLessThanStart = LitString<43>::FromCString("The end index is less than the start index.");
	LitString<44> _HashKeyNotFound = LitString<44>::FromCString("The hash does not contain the specified key.");
	LitString<46> _RadixOutOfRange = LitString<46>::FromCString("The radix must be between 2 and 36, inclusive.");
	LitString<30> _InvalidIntegerFormat = LitString<30>::FromCString("Invalid integer format string.");
	LitString<32> _FileHandleClosed = LitString<32>::FromCString("The file handle has been closed.");
	LitString<54> _AppendMustBeWriteOnly = LitString<54>::FromCString("A file opened for appending must use FileAccess.write.");
	LitString<32> _CannotFlushReadOnlyStream = LitString<32>::FromCString("Cannot flush a read-only stream.");
	LitString<63> _FileStreamWithNonFile = LitString<63>::FromCString("A FileStream cannot be used to open a file that is not on disk.");
	LitString<42> _EncodingBufferOverrun = LitString<42>::FromCString("Cannot write beyond the end of the buffer.");
	LitString<37> _ValueNotInvokable = LitString<37>::FromCString("The specified value is not invokable.");

	String *EndIndexLessThanStart     = _EndIndexLessThanStart.AsString();
	String *HashKeyNotFound           = _HashKeyNotFound.AsString();
	String *RadixOutOfRange           = _RadixOutOfRange.AsString();
	String *InvalidIntegerFormat      = _InvalidIntegerFormat.AsString();
	String *FileHandleClosed          = _FileHandleClosed.AsString();
	String *AppendMustBeWriteOnly     = _AppendMustBeWriteOnly.AsString();
	String *CannotFlushReadOnlyStream = _CannotFlushReadOnlyStream.AsString();
	String *FileStreamWithNonFile     = _FileStreamWithNonFile.AsString();
	String *EncodingBufferOverrun     = _EncodingBufferOverrun.AsString();
	String *ValueNotInvokable         = _ValueNotInvokable.AsString();
}
