#ifndef AVES__TIMER_H
#define AVES__TIMER_H

#include "../aves.h"

namespace aves
{
	class Stopwatch
	{
	public:
		bool isRunning;

		// Current (saved) elapsed time, in microseconds.
		int64_t elapsed;

		// The monotonic clock time that the stopwatch was started. If the stopwatch is
		// not running, this field is zero.
		int64_t startTime;

		static void GetMonotonicClock(int64_t &result);

		static void Init();

	private:
		// The frequency of the monotonic clock, in ticks per second.
		static int64_t clockFrequency;
	};
}

AVES_API int aves_Stopwatch_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Stopwatch_new);

AVES_API NATIVE_FUNCTION(aves_Stopwatch_get_isRunning);
AVES_API NATIVE_FUNCTION(aves_Stopwatch_get_elapsed);

AVES_API NATIVE_FUNCTION(aves_Stopwatch_reset);
AVES_API NATIVE_FUNCTION(aves_Stopwatch_start);
AVES_API NATIVE_FUNCTION(aves_Stopwatch_stop);
AVES_API NATIVE_FUNCTION(aves_Stopwatch_restart);
AVES_API NATIVE_FUNCTION(aves_Stopwatch_startNew);

#endif // AVES__TIMER_H