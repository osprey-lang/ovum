#pragma once

#include "../vm.h"
#include <atomic>

namespace ovum
{

// CriticalSection is a recursive mutex: it can be entered by one thread at a
// time, but that thread can enter the critical section any number of times.
// When the owning thread has called Leave() as many times as it has called
// Enter(), other threads are free to enter the same section.
class CriticalSection
{
public:
	inline explicit CriticalSection(int spinCount)
	{
		os::CriticalSectionInit(&cs, spinCount);
	}

	inline ~CriticalSection()
	{
		os::CriticalSectionDestroy(&cs);
	}

	// Enters a critical section. The calling thread will block until the critical
	// section has been entered.
	// Returns:
	//   OVUM_SUCCESS if the critical section was successfully entered, or an
	//   error code otherwise. This method will never return OVUM_ERROR_BUSY.
	inline int Enter()
	{
		return os::CriticalSectionEnter(&cs);
	}

	// Attempts to enter a critical section. This method returns immediately.
	// Returns:
	//   If another thread is inside the critical section, the method returns
	//   OVUM_ERROR_BUSY. If an error occurs, an error code is returned.
	//   Otherwise, if the critical section was entered, the method returns
	//   OVUM_SUCCESS.
	inline int TryEnter()
	{
		return os::CriticalSectionTryEnter(&cs);
	}

	// Leaves the critical section. Other threads are now free to enter it.
	// Returns:
	//   If an error occurs, an error code. Otherwise, OVUM_SUCCESS.
	inline int Leave()
	{
		return os::CriticalSectionLeave(&cs);
	}

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(CriticalSection);

	os::CriticalSection cs;
};

class Semaphore
{
public:
	inline explicit Semaphore(int value)
	{
		os::SemaphoreInit(&semaphore, value);
	}

	inline ~Semaphore()
	{
		os::SemaphoreDestroy(&semaphore);
	}

	// Decrements the semaphore value by one. If the value is currently zero,
	// the calling thread will block until another thread increments the
	// semaphore count.
	// Returns:
	//   OVUM_SUCCESS if the semaphore was successfully entered, or an error
	//   code otherwise. This function will never return OVUM_ERROR_BUSY.
	inline int Enter()
	{
		return os::SemaphoreEnter(&semaphore);
	}

	// Attempts to decrement the semaphore value by one. If the value is
	// currently zero, the function returns without affecting the semaphore.
	// This function returns immediately.
	// Returns:
	//   If the semaphore is was zero, the function returns OVUM_ERROR_BUSY.
	//   If the semaphore was decremented, the return value is OVUM_SUCCESS.
	//   Otherwise, an error code is returned.
	inline int TryEnter()
	{
		return os::SemaphoreTryEnter(&semaphore);
	}

	// Increments the semaphore by one.
	// Returns:
	//   If the semaphore was successfully incremented, the return value is
	//   OVUM_SUCCESS. Otherwise, an error code is returned.
	inline int Leave()
	{
		return os::SemaphoreLeave(&semaphore);
	}

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(Semaphore);

	os::Semaphore semaphore;
};

// SpinLock is a simple, non-recursive lock. Attempting to enter the lock when
// it is taken by another thread will cause the lock to spin - that is, try to
// acquire the lock in a loop until it succeeds.
//
// Spinlocks should only be held for a very short amount of time.
//
// Spinlocks are NOT recursive: it is not possible to enter the same lock multiple
// times on the same thread. Attempting to do so will result in a deadlock.
class SpinLock
{
public:
	inline SpinLock()
	{
		flag.clear(std::memory_order_release);
	}

	// Enters the spinlock. If the lock is already held, the thread will spin
	// until it becomes available.
	inline void Enter()
	{
		// This method is kept as minimal as possible to encourage inlining.
		// It is optimised for the common case of an uncontested lock; only
		// if the lock is busy do we bother spinning.
		if (flag.test_and_set(std::memory_order_acquire))
			SpinWait();
	}

	// Tries to enter the spinlock. This method returns immediately; if the
	// return value is true, the lock was successfully entered. Otherwise,
	// the lock is already held.
	inline bool TryEnter()
	{
		return flag.test_and_set(std::memory_order_acquire) == false;
	}

	// Leaves the spinlock, enabling another thread to enter it.
	inline void Leave()
	{
		flag.clear(std::memory_order_release);
	}

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(SpinLock);

	// The total number of times to spin before yielding
	static const int MAX_COUNT_BEFORE_YIELDING = 100;

	std::atomic_flag flag;

	OVUM_NOINLINE void SpinWait();
};

} // namespace ovum
