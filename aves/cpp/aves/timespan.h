#ifndef AVES__TIMESPAN_H
#define AVES__TIMESPAN_H

#include "../aves.h"

namespace aves
{
	class TimeSpan
	{
	public:
		static const int64_t MAX_MILLIS = INT64_MAX / 1000;
		static const int64_t MIN_MILLIS = INT64_MIN / 1000;

		static const int64_t MILLIS_PER_SECOND;
		static const int64_t MILLIS_PER_MINUTE;
		static const int64_t MILLIS_PER_HOUR;
		static const int64_t MILLIS_PER_DAY;

		template<int64_t Factor>
		static int ToMilliseconds(ThreadHandle thread, Value *value, int64_t &microseconds);
	};
}

AVES_API int aves_TimeSpan_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_TimeSpan_newMicros);
AVES_API NATIVE_FUNCTION(aves_TimeSpan_newHms);
AVES_API NATIVE_FUNCTION(aves_TimeSpan_newDhms);
AVES_API NATIVE_FUNCTION(aves_TimeSpan_newDhmsMillis);

AVES_API NATIVE_FUNCTION(aves_TimeSpan_get_rawValue);

#endif // AVES__TIMESPAN_H