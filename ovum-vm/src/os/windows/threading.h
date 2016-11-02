#pragma once

#include "../../../inc/ovum.h"
#include "def.h"
#include <limits.h>

namespace ovum
{

namespace os
{

	typedef DWORD ThreadId;
	typedef CRITICAL_SECTION CriticalSection;
	typedef HANDLE Semaphore;
	typedef DWORD TlsKey;

	static const ThreadId INVALID_THREAD_ID = 0;

	// Gets the ID of the current thread.
	inline ThreadId GetCurrentThread()
	{
		return ::GetCurrentThreadId();
	}

	// Causes the calling thread to yield execution to another thread
	// that is ready to run. This may or may not actually cause another
	// thread to run, depending entirely on the OS's thread scheduling.
	// This function can, under complicated circumstances involving,
	// asynchronous I/O, occasionally cause the process to deadlock.
	inline void Yield()
	{
		::SwitchToThread();
	}

	// Suspends the calling thread for a number of milliseconds. The
	// thread does not wake up until the specified time has elapsed.
	// Returns:
	//   False if the sleep was interrupted (e.g. by a signal); true
	//   if the thread slept peacefully. Depending on the OS, thread
	//   sleep may or may not be interruptible.
	inline bool Sleep(uint32_t milliseconds)
	{
		::Sleep((DWORD)milliseconds);
		return true;
	}

	// Attempts to initialize a critical section. The spin count
	// may be ignored on some platforms. Returns true if successful;
	// otherwise, false.
	inline bool CriticalSectionInit(CriticalSection *cs, int spinCount)
	{
		return ::InitializeCriticalSectionEx(
			cs,
			spinCount,
			CRITICAL_SECTION_NO_DEBUG_INFO
		) != 0;
	}

	// Destroys a critical section.
	inline void CriticalSectionDestroy(CriticalSection *cs)
	{
		::DeleteCriticalSection(cs);
	}

	// Enters a critical section. The calling thread will block
	// until the critical section has been entered.
	// Returns:
	//   OVUM_SUCCESS if the critical section was successfully entered,
	//   or an error code otherwise. This function will never return
	//   OVUM_ERROR_BUSY.
	inline int CriticalSectionEnter(CriticalSection *cs)
	{
		::EnterCriticalSection(cs);
		RETURN_SUCCESS;
	}

	// Attempts to enter a critical section. If the critical section
	// is unavailable, this function returns immediately.
	// Returns:
	//   If another thread is inside the critical section, the function
	//   returns OVUM_ERROR_BUSY. If the critical section was entered
	//   successfully, the return value is OVUM_SUCCESS. Otherwise, an
	//   error code is returned.
	inline int CriticalSectionTryEnter(CriticalSection *cs)
	{
		if (::TryEnterCriticalSection(cs))
			RETURN_SUCCESS;
		return OVUM_ERROR_BUSY;
	}

	// Leaves a critical section, which must be owned by the calling thread.
	inline int CriticalSectionLeave(CriticalSection *cs)
	{
		::LeaveCriticalSection(cs);
		RETURN_SUCCESS;
	}


	// Attempts to initialize a semaphore. The semaphore is given
	// the specified initial value. Returns true if successful;
	// otherwise, false.
	inline bool SemaphoreInit(Semaphore *sem, int value)
	{
		HANDLE h = ::CreateSemaphoreW(
			nullptr,
			value,
			INT_MAX,
			nullptr
		);
		if (h != nullptr)
		{
			*sem = h;
			return true;
		}
		return false;
	}

	// Destroys a semaphore.
	inline void SemaphoreDestroy(Semaphore *sem)
	{
		::CloseHandle(*sem);
	}

	// Decrements the semaphore value by one. If the value is currently
	// zero, the calling thread will block until another thread increments
	// the semaphore count.
	// Returns:
	//   OVUM_SUCCESS if the semaphore was successfully entered, or an
	//   error code otherwise. This function will never return OVUM_ERROR_BUSY.
	inline int SemaphoreEnter(Semaphore *sem)
	{
		int r = ::WaitForSingleObject(*sem, INFINITE);
		if (r != 0)
			return OVUM_ERROR_UNSPECIFIED;
		RETURN_SUCCESS;
	}

	// Attempts to decrement the semaphore value by one. If the value is
	// currently zero, the function returns without affecting the semaphore.
	// This function always returns immediately.
	// Returns:
	//   If the semaphore is was zero, the function returns OVUM_ERROR_BUSY.
	//   If the semaphore was decremented, the return value is OVUM_SUCCESS.
	//   Otherwise, an error code is returned.
	inline int SemaphoreTryEnter(Semaphore *sem)
	{
		int r = ::WaitForSingleObject(*sem, 0);
		if (r != 0)
		{
			if (r == WAIT_TIMEOUT)
				return OVUM_ERROR_BUSY;
			return OVUM_ERROR_UNSPECIFIED;
		}
		RETURN_SUCCESS;
	}

	// Increments the semaphore by one.
	// Returns:
	//   If the semaphore was successfully incremented, the return value is
	//   OVUM_SUCCESS. Otherwise, an error code is returned.
	inline int SemaphoreLeave(Semaphore *sem)
	{
		BOOL r = ::ReleaseSemaphore(*sem, 1, nullptr);
		if (!r)
			return OVUM_ERROR_UNSPECIFIED;
		RETURN_SUCCESS;
	}


	// Attempts to allocate a TLS key. Returns true if successful;
	// otherwise, false.
	inline bool TlsAlloc(TlsKey *key)
	{
		TlsKey k = ::TlsAlloc();
		if (k != TLS_OUT_OF_INDEXES)
		{
			*key = k;
			return true;
		}
		return false;
	}

	// Frees the specified TLS key. This does not free the value
	// stored in the key in any way.
	inline void TlsFree(TlsKey *key)
	{
		::TlsFree(*key);
	}

	// Gets the value stored in the specified TLS key.
	inline void *const TlsGet(TlsKey *key)
	{
		return ::TlsGetValue(*key);
	}

	// Sets the value of the specified TLS key.
	inline void TlsSet(TlsKey *key, void *const value)
	{
		::TlsSetValue(*key, value);
	}

} // namespace os

} // namespace ovum
