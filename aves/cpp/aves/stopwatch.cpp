#include "stopwatch.h"
#include "../aves_state.h"

namespace aves
{

	int64_t Stopwatch::clockFrequency;

	void Stopwatch::GetMonotonicClock(int64_t &result)
	{
		LARGE_INTEGER clock;
		// QueryPerformanceCounter does not fail on Windows XP and later.
		QueryPerformanceCounter(&clock);

		// Convert to microseconds first, to avoid loss of precision.
		clock.QuadPart *= 1000000;
		clock.QuadPart /= clockFrequency;
		result = clock.QuadPart;
	}

	void Stopwatch::Init()
	{
		// Get the monotonic clock frequency. This is fixed at system boot, so can be
		// cached for the duration of the process.
		LARGE_INTEGER frequency;

		// QueryPerformanceFrequency does not fail on Windows XP and later.
		QueryPerformanceFrequency(&frequency);

		clockFrequency = frequency.QuadPart;
	}

} // namespace aves

using namespace aves;

AVES_API int aves_Stopwatch_init(TypeHandle type)
{
	Stopwatch::Init();
	Type_SetInstanceSize(type, sizeof(aves::Stopwatch));

	// Stopwatch has no managed references, so nothing else to do here.
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Stopwatch_new)
{
	// new()
	// GC initializes all bytes to zero. Nothing to do here!
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Stopwatch_get_isRunning)
{
	Stopwatch *stopwatch = THISV.Get<Stopwatch>();
	VM_PushBool(thread, stopwatch->isRunning);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Stopwatch_get_elapsed)
{
	Stopwatch *stopwatch = THISV.Get<Stopwatch>();

	int64_t elapsed;
	if (stopwatch->isRunning)
	{
		int64_t currentClock;
		Stopwatch::GetMonotonicClock(currentClock);

		currentClock -= stopwatch->startTime;
		elapsed = currentClock + stopwatch->elapsed;
	}
	else
	{
		elapsed = stopwatch->elapsed;
	}

	// Obtain the static state only /after/ calculating the current elapsed
	// time. We want as few method calls as possible, to avoid performance
	// penalties.
	Aves *aves = Aves::Get(thread);

	Value timeSpan;
	timeSpan.type = aves->aves.TimeSpan;
	timeSpan.v.integer = elapsed;
	VM_Push(thread, &timeSpan);

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Stopwatch_reset)
{
	Stopwatch *stopwatch = THISV.Get<Stopwatch>();
	stopwatch->isRunning = false;
	stopwatch->elapsed = 0;
	stopwatch->startTime = 0;
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Stopwatch_start)
{
	Stopwatch *stopwatch = THISV.Get<Stopwatch>();
	if (!stopwatch->isRunning)
	{
		stopwatch->isRunning = true;
		Stopwatch::GetMonotonicClock(stopwatch->startTime);
	}
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Stopwatch_stop)
{
	Stopwatch *stopwatch = THISV.Get<Stopwatch>();
	if (stopwatch->isRunning)
	{
		// Calculate the time that has elapsed since the stopwatch was last started,
		// and add that to timer->elapsed.
		int64_t currentClock;
		Stopwatch::GetMonotonicClock(currentClock);

		currentClock -= stopwatch->startTime;
		stopwatch->elapsed += currentClock;

		stopwatch->isRunning = false;
		stopwatch->startTime = 0;
	}
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Stopwatch_restart)
{
	Stopwatch *stopwatch = THISV.Get<Stopwatch>();
	stopwatch->isRunning = true;
	Stopwatch::GetMonotonicClock(stopwatch->startTime);
	stopwatch->elapsed = 0;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Stopwatch_startNew)
{
	Aves *aves = Aves::Get(thread);

	Value stopwatchValue;
	CHECKED(GC_Construct(thread, aves->aves.Stopwatch, 0, &stopwatchValue));
	VM_Push(thread, &stopwatchValue);

	Stopwatch *stopwatch = stopwatchValue.Get<Stopwatch>();
	stopwatch->isRunning = true;
	Stopwatch::GetMonotonicClock(stopwatch->startTime);

	// The stopwatch is on top of the stack, so just return here.
}
END_NATIVE_FUNCTION
