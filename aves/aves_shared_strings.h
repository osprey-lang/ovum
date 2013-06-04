#pragma once

#ifndef AVES__SHARED_STRINGS_H
#define AVES__SHARED_STRINGS_H

#include "aves.h"

namespace strings
{
	extern String *format;
	extern String *toString;

	extern String *str;
	extern String *i;
	extern String *cur;
	extern String *index;
	extern String *capacity;
}

namespace error_strings
{
	extern String *EndIndexLessThanStart;
}

#endif // AVES__SHARED_STRINGS_H