#pragma once

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
	extern String *_args;
	extern String *x;
	extern String *y;
	extern String *width;
	extern String *height;
	extern String *add;

	extern String *newline;
}

namespace error_strings
{
	extern String *EndIndexLessThanStart;
	extern String *HashKeyNotFound;
}

#endif // AVES__SHARED_STRINGS_H