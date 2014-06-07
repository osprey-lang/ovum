#pragma once

#ifndef VM__CRITICAL_SECTION_H
#define VM__CRITICAL_SECTION_H

#include "ov_vm.internal.h"
#include <atomic>

// CriticalSection works like a recursive mutex: it can be entered
// by one thread at a time, but that thread can enter the critical
// section any number of times. When the owning thread has called
// Leave() as many times as it has called Enter(), it enables other
// threads to enter the same section.
//
// DO NOT copy CriticalSection instances by value, only ever by
// reference or pointer. Critical sections should usually only be
// accessed directly through the containing field.
class CriticalSection
{
private:
	CRITICAL_SECTION section;

public:
	inline CriticalSection(uint32_t spinCount)
	{
		InitializeCriticalSectionEx(&section, spinCount,
			CRITICAL_SECTION_NO_DEBUG_INFO);
	}

	inline ~CriticalSection()
	{
		DeleteCriticalSection(&section);
	}

	// Enters the critical section. If another thread has
	// entered it already, the current thread blocks until
	// the section becomes available.
	inline void Enter()
	{
		EnterCriticalSection(&section);
	}

	// Tries to enter the critical section. This method
	// always returns immediately.
	// If this thread successfully entered the section,
	// true is returned; otherwise, another thread has
	// already entered the section, and false is returned.
	inline bool TryEnter()
	{
		return TryEnterCriticalSection(&section) != 0;
	}

	// Leaves the critical section. Other threads are now
	// free to enter it.
	inline void Leave()
	{
		LeaveCriticalSection(&section);
	}
};

// SpinLock is a simple, non-recursive lock. Attempting to enter
// the lock when it is taken by another thread will cause the lock
// to spin, that is, repeatedly try to acquire the lock in a loop.
//
// Spinlocks should only be held for a very short amount of time.
//
// DO NOT copy SpinLock instances by value, only ever by reference
// or pointer. Spinlocks should usually only be accessed directly
// through the containing field.
class SpinLock
{
private:
	std::atomic_flag flag;

public:
	inline SpinLock() : flag() { }

	// Enters the spinlock. If the lock is already held,
	// the thread will spin until it becomes available.
	inline void Enter()
	{
		while (flag.test_and_set(std::memory_order_acquire))
			; // Spin!
	}

	// Tries to enter the spinlock. This method returns
	// immediately; if the return value is true, the lock
	// was successfully entered. Otherwise, the lock is
	// already held.
	inline bool TryEnter()
	{
		return flag.test_and_set(std::memory_order_acquire) == false;
	}

	// Leaves the spinlock, enabling another thread to
	// enter it.
	inline void Leave()
	{
		flag.clear(std::memory_order_release);
	}
};

#endif // VM__CRITICAL_SECTION_H