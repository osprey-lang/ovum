#include "sync.h"

namespace ovum
{

void SpinLock::SpinWait()
{
	int spinCountLeft = MAX_COUNT_BEFORE_YIELDING;
	while (spinCountLeft != 0)
	{
		if (!flag.test_and_set(std::memory_order_acquire))
			return;
		spinCountLeft--;
	}

	// Since the above loop makes the thread quite active, it may actually
	// prevent other threads from executing. After a few spins, we yield our
	// timeslice and hopefully allow other threads to finish execution long
	// enough to release the lock.
	while (true)
	{
		if (!flag.test_and_set(std::memory_order_acquire))
			return;
		os::Yield();
	}
}

} // namespace ovum
