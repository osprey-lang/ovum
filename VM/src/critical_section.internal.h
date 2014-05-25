#pragma once

#ifndef VM__CRITICAL_SECTION_H
#define VM__CRITICAL_SECTION_H

#include "ov_vm.internal.h"

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

#endif // VM__CRITICAL_SECTION_H