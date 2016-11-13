#ifndef AVES__STATE_H
#define AVES__STATE_H

#include "aves.h"
#include "aves/console.h"
#include "aves/env.h"

namespace aves
{

class Aves
{
public:
	static Aves *Get(ThreadHandle thread);

	static bool Init(ModuleHandle module);

	// These fields are public only because they're used extremely frequently.
	// Organised by Osprey namespace, for ease of remembering.

	struct AvesTypes
	{
		TypeHandle ArgumentError;
		TypeHandle ArgumentNullError;
		TypeHandle ArgumentRangeError;
		TypeHandle ArgumentTypeError;
		TypeHandle Array;
		TypeHandle Buffer;
		TypeHandle BufferViewKind;
		TypeHandle Char;
		TypeHandle ConsoleColor;
		TypeHandle ConsoleKey;
		TypeHandle ConsoleKeyCode;
		TypeHandle DuplicateKeyError;
		TypeHandle HashEntry;
		TypeHandle Int;
		TypeHandle InvalidStateError;
		TypeHandle NotSupportedError;
		TypeHandle Real;
		TypeHandle Stopwatch;
		TypeHandle String;
		TypeHandle TimeSpan;
		TypeHandle UInt;
		TypeHandle UnicodeCategory;
		TypeHandle Version;

		struct ReflectionTypes
		{
			TypeHandle Accessibility;
			TypeHandle Constructor;
			TypeHandle Field;
			TypeHandle GlobalConstant;
			TypeHandle MemberSearchFlags;
			TypeHandle Method;
			TypeHandle Module;
			TypeHandle NativeHandle;
			TypeHandle Overload;
			TypeHandle Property;
		} reflection;
	} aves;

	struct IoTypes
	{
		TypeHandle FileNotFoundError;
		TypeHandle IOError;
	} io;

private:
	Aves();
	~Aves();

	void InitTypes(ModuleHandle module);

	static void Deallocate(void *state);
};

} // namespace aves

#endif // AVES__STATE_H
