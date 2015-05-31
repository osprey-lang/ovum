#ifndef AVES__SHARED_STRINGS_H
#define AVES__SHARED_STRINGS_H

#include "aves.h"

namespace strings
{
	extern String *Empty;

	extern String *format;
	extern String *toString;

	extern String *str;
	extern String *i;
	extern String *cur;
	extern String *index;
	extern String *capacity;
	extern String *values;
	extern String *value;
	extern String *times;
	extern String *oldValue;
	extern String *kind;
	extern String *key;
	extern String *cp;
	extern String *_args; // _args
	extern String *x;
	extern String *y;
	extern String *width;
	extern String *height;
	extern String *add;
	extern String *path;
	extern String *mode;
	extern String *access;
	extern String *share;
	extern String *origin;
	extern String *handle;
	extern String *flags;
	extern String *overload;
	extern String *major;
	extern String *minor;
	extern String *build;
	extern String *revision;
	extern String *minLength;
	extern String *side;
	extern String *size;
	extern String *_call; // .call
	extern String *_iter; // .iter
	extern String *_new;  // .new
	
	extern String *newline;
}

namespace error_strings
{
	extern String *EndIndexLessThanStart;
	extern String *HashKeyNotFound;
	extern String *RadixOutOfRange;
	extern String *InvalidIntegerFormat;
	extern String *FileHandleClosed;
	extern String *AppendMustBeWriteOnly;
	extern String *CannotFlushReadOnlyStream;
	extern String *FileStreamWithNonFile;
	extern String *EncodingBufferOverrun;
	extern String *ValueNotInvokable;
}

#endif // AVES__SHARED_STRINGS_H