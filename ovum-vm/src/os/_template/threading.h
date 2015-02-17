#pragma once

#ifndef VM__OS_THREADING_H
#define VM__OS_THREADING_H

#include "../../../inc/ov_vm.h"
#include "def.h"

namespace ovum
{

namespace os
{

	// Thread identifier type. This must be a type that can be copied
	// safely by value.
	typedef ... ThreadId;
	typedef ... CriticalSection;
	typedef ... Semaphore;
	typedef ... TlsKey;

	static const ThreadId INVALID_THREAD_ID = ...;
	
	// Gets the ID of the current thread.
	ThreadId GetCurrentThread();

	// Causes the calling thread to yield execution to another thread
	// that is ready to run. This may or may not actually cause another
	// thread to run, depending entirely on the OS's thread scheduling.
	// This function can, under complicated circumstances involving,
	// asynchronous I/O, occasionally cause the process to deadlock.
	void Yield();

	// Suspends the calling thread for a number of milliseconds. The
	// thread does not wake up until the specified time has elapsed.
	// Returns:
	//   False if the sleep was interrupted (e.g. by a signal); true
	//   if the thread slept peacefully. Depending on the OS, thread
	//   sleep may or may not be interruptible.
	bool Sleep(uint32_t milliseconds);

	// Attempts to initialize a critical section. The spin count
	// may be ignored on some platforms. Returns true if successful;
	// otherwise, false.
	bool CriticalSectionInit(CriticalSection *cs, int spinCount);

	// Destroys a critical section.
	void CriticalSectionDestroy(CriticalSection *cs);

	// Enters a critical section. The calling thread will block until
	// the critical section has been entered.
	// Returns:
	//   OVUM_SUCCESS if the critical section was successfully entered,
	//   or an error code otherwise. This function will never return
	//   OVUM_ERROR_BUSY.
	int CriticalSectionEnter(CriticalSection *cs);

	// Attempts to enter a critical section. This function always
	// returns immediately.
	// Returns:
	//   If another thread is inside the critical section, the function
	//   returns OVUM_ERROR_BUSY. If the critical section was entered
	//   successfully, the return value is OVUM_SUCCESS. Otherwise, an
	//   error code is returned.
	int CriticalSectionTryEnter(CriticalSection *cs);

	// Leaves a critical section, which must be owned by the calling thread.
	int CriticalSectionLeave(CriticalSection *cs);


	// Attempts to initialize a semaphore. The semaphore is given
	// the specified initial value. Returns true if successful;
	// otherwise, false.
	bool SemaphoreInit(Semaphore *sem, int value);

	// Destroys a semaphore.
	void SemaphoreDestroy(Semaphore *sem);

	// Decrements the semaphore value by one. If the value is currently
	// zero, the calling thread will block until another thread increments
	// the semaphore count.
	// Returns:
	//   OVUM_SUCCESS if the semaphore was successfully entered, or an
	//   error code otherwise. This function will never return OVUM_ERROR_BUSY.
	int SemaphoreEnter(Semaphore *sem);

	// Attempts to decrement the semaphore value by one. If the value is
	// currently zero, the function returns without affecting the semaphore.
	// This function always returns immediately.
	// Returns:
	//   If the semaphore is was zero, the function returns OVUM_ERROR_BUSY.
	//   If the semaphore was decremented, the return value is OVUM_SUCCESS.
	//   Otherwise, an error code is returned.
	int SemaphoreTryEnter(Semaphore *sem);

	// Increments the semaphore by one.
	// Returns:
	//   If the semaphore was successfully incremented, the return value is
	//   OVUM_SUCCESS. Otherwise, an error code is returned.
	int SemaphoreLeave(Semaphore *sem);


	// Attempts to allocate a TLS key. Returns true if successful;
	// otherwise, false.
	bool TlsAlloc(TlsKey *key);

	// Frees the specified TLS key. This does not free the value
	// stored in the key in any way.
	void TlsFree(TlsKey *key);

	// Gets the value stored in the specified TLS key.
	void *const TlsGet(TlsKey *key);

	// Sets the value of the specified TLS key.
	void TlsSet(TlsKey *key, void *const value);

} // namespace os

} // namespace ovum

#endif // VM__OS_THREADING_H