#pragma once

#ifndef VM__CRITICAL_SECTION_H
#define VM__CRITICAL_SECTION_H

#include "ov_vm.internal.h"

#include <atomic>
#if OVUM_TARGET == OVUM_UNIX
#include <pthread.h>
#include <semaphore.h>
#endif

namespace ovum
{

// CriticalSection works like a recursive mutex: it can be entered
// by one thread at a time, but that thread can enter the critical
// section any number of times. When the owning thread has called
// Leave() as many times as it has called Enter(), other threads
// are free to enter the same section.
//
// DO NOT copy CriticalSection instances by value, only ever by
// reference or pointer. Critical sections should usually only be
// accessed directly through the containing field.
class CriticalSection
{
private:
#if OVUM_TARGET == OVUM_WINDOWS
	CRITICAL_SECTION section;
#else
	pthread_mutex_t mutex;
#endif

	DISABLE_COPY_AND_ASSIGN(CriticalSection);

public:
	inline CriticalSection(uint32_t spinCount)
	{
#if OVUM_TARGET == OVUM_WINDOWS
		InitializeCriticalSectionEx(&section, spinCount,
			CRITICAL_SECTION_NO_DEBUG_INFO);
#else
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mutex, &attr);
		pthread_mutexattr_destroy(&attr);
#endif
	}

	inline ~CriticalSection()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		DeleteCriticalSection(&section);
#else
		pthread_mutex_destroy(&mutex);
#endif
	}

	// Enters the critical section. If another thread has
	// entered it already, the current thread blocks until
	// the section becomes available.
	inline void Enter()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		EnterCriticalSection(&section);
#else
		pthread_mutex_lock(&mutex);
#endif
	}

	// Tries to enter the critical section. This method
	// always returns immediately.
	// If this thread successfully entered the section,
	// true is returned; otherwise, another thread has
	// already entered the section, and false is returned.
	inline bool TryEnter()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		return TryEnterCriticalSection(&section) != 0;
#else
		return pthread_mutex_trylock(&mutex) == 0;
#endif
	}

	// Leaves the critical section. Other threads are now
	// free to enter it.
	inline void Leave()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		LeaveCriticalSection(&section);
#else
		pthread_mutex_unlock(&mutex);
#endif
	}
};

class Semaphore
{
private:
#if OVUM_TARGET == OVUM_WINDOWS
	HANDLE semaphore;
#else
	sem_t semaphore;
#endif

	DISABLE_COPY_AND_ASSIGN(Semaphore);

public:
	inline Semaphore(int value)
	{
#if OVUM_TARGET == OVUM_WINDOWS
		semaphore = CreateSemaphoreW(nullptr,
			value,
			INT_MAX,
			nullptr);
#else
		sem_init(&semaphore, 0, value);
#endif
	}

	inline ~Semaphore()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		CloseHandle(semaphore);
#else
		sem_destroy(&semaphore);
#endif
	}

	// Decrements the semaphore value by one. If the value is
	// currently zero, the calling thread will block until another
	// thread increments the semaphore count.
	inline void Enter()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		WaitForSingleObject(semaphore, INFINITE);
#else
		// TODO: Handle signal interrupts
		sem_wait(&semaphore);
#endif
	}

	inline bool TryEnter()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		return WaitForSingleObject(semaphore, 0) != ERROR_TIMEOUT;
#else
		return sem_trywait(&semaphore) != EAGAIN;
#endif
	}

	inline void Leave()
	{
#if OVUM_TARGET == OVUM_WINDOWS
		ReleaseSemaphore(semaphore, 1, nullptr);
#else
		sem_post(&semaphore);
#endif
	}
};

// SpinLock is a simple, non-recursive lock. Attempting to enter
// the lock when it is taken by another thread will cause the lock
// to spin, that is, repeatedly try to acquire the lock in a loop.
//
// Spinlocks should only be held for a very short amount of time.
//
// Spinlocks are NOT recursive: it is not possible to enter the same
// lock multiple times on the same thread. Attempting to do so will
// result in a deadlock.
//
// DO NOT copy SpinLock instances by value, only ever by reference
// or pointer. Spinlocks should usually only be accessed directly
// through the containing field.
class SpinLock
{
private:
	std::atomic_flag flag;

	DISABLE_COPY_AND_ASSIGN(SpinLock);

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

} // namespace ovum

#endif // VM__CRITICAL_SECTION_H