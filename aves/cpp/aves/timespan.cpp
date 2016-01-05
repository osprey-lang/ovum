#include "timespan.h"
#include "../aves_state.h"

namespace aves
{
	const int64_t TimeSpan::MILLIS_PER_SECOND = 1000;
	const int64_t TimeSpan::MILLIS_PER_MINUTE = 60 * MILLIS_PER_SECOND;
	const int64_t TimeSpan::MILLIS_PER_HOUR = 60 * MILLIS_PER_MINUTE;
	const int64_t TimeSpan::MILLIS_PER_DAY = 24 * MILLIS_PER_HOUR;

	template<int64_t Factor>
	int TimeSpan::ToMilliseconds(ThreadHandle thread, Value *value, int64_t &result)
	{
		Aves *aves = Aves::Get(thread);

		if (value->type == aves->aves.Real)
		{
			double fraction = value->v.real;
			result = (int64_t)(fraction * Factor);
		}
		else
		{
			int r = IntFromValue(thread, value);
			if (r != OVUM_SUCCESS)
				return r;

			r = Int_MultiplyChecked(value->v.integer, Factor, result);
			if (r != OVUM_SUCCESS)
				return VM_ThrowOverflowError(thread);
		}

		RETURN_SUCCESS;
	}
}

using namespace aves;

AVES_API int aves_TimeSpan_init(TypeHandle type)
{
	Type_SetConstructorIsAllocator(type, true);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_TimeSpan_newMicros)
{
	// new(microseconds)
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));

	Value timeSpan;
	timeSpan.type = aves->aves.TimeSpan;
	timeSpan.v.integer = args[1].v.integer;
	VM_Push(thread, &timeSpan);
}
END_NATIVE_FUNCTION;

AVES_API BEGIN_NATIVE_FUNCTION(aves_TimeSpan_newHms)
{
	// new(hours, minutes, seconds)
	Aves *aves = Aves::Get(thread);

	int64_t hours, minutes, seconds;
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_HOUR>(thread, args + 1, hours));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_MINUTE>(thread, args + 2, minutes));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_SECOND>(thread, args + 3, seconds));

	int64_t millis = 0;
	int r = Int_AddChecked(hours, minutes, millis);
	r |= Int_AddChecked(millis, seconds, millis);
	if (r != OVUM_SUCCESS)
		return VM_ThrowOverflowError(thread);

	if (millis < TimeSpan::MIN_MILLIS || millis > TimeSpan::MAX_MILLIS)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);

	Value timeSpan;
	timeSpan.type = aves->aves.TimeSpan;
	timeSpan.v.integer = millis * 1000;
	VM_Push(thread, &timeSpan);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_TimeSpan_newDhms)
{
	// new(days, hours, minutes, seconds)
	Aves *aves = Aves::Get(thread);

	int64_t days, hours, minutes, seconds;
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_DAY>(thread, args + 1, days));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_HOUR>(thread, args + 2, hours));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_MINUTE>(thread, args + 3, minutes));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_SECOND>(thread, args + 4, seconds));

	int64_t millis = 0;
	int r = Int_AddChecked(days, hours, millis);
	r |= Int_AddChecked(millis, minutes, millis);
	r |= Int_AddChecked(millis, seconds, millis);
	if (r != OVUM_SUCCESS)
		return VM_ThrowOverflowError(thread);

	if (millis < TimeSpan::MIN_MILLIS || millis > TimeSpan::MAX_MILLIS)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);

	Value timeSpan;
	timeSpan.type = aves->aves.TimeSpan;
	timeSpan.v.integer = millis * 1000;
	VM_Push(thread, &timeSpan);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_TimeSpan_newDhmsMillis)
{
	static const int64_t MILLIS_PER_MILLI = 1;

	// new(days, hours, minutes, seconds, milliseconds)
	Aves *aves = Aves::Get(thread);

	int64_t days, hours, minutes, seconds, millis;
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_DAY>(thread, args + 1, days));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_HOUR>(thread, args + 2, hours));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_MINUTE>(thread, args + 3, minutes));
	CHECKED(TimeSpan::ToMilliseconds<TimeSpan::MILLIS_PER_SECOND>(thread, args + 4, seconds));
	CHECKED(TimeSpan::ToMilliseconds<MILLIS_PER_MILLI>(thread, args + 5, millis));

	int r = Int_AddChecked(millis, days, millis);
	r |= Int_AddChecked(millis, hours, millis);
	r |= Int_AddChecked(millis, minutes, millis);
	r |= Int_AddChecked(millis, seconds, millis);
	if (r != OVUM_SUCCESS)
		return VM_ThrowOverflowError(thread);

	if (millis < TimeSpan::MIN_MILLIS || millis > TimeSpan::MAX_MILLIS)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);

	Value timeSpan;
	timeSpan.type = aves->aves.TimeSpan;
	timeSpan.v.integer = millis * 1000;
	VM_Push(thread, &timeSpan);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_TimeSpan_get_rawValue)
{
	VM_PushInt(thread, THISV.v.integer);
	RETURN_SUCCESS;
}
