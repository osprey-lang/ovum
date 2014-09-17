#include "tls.internal.h"

#if OVUM_TARGET == OVUM_UNIX
#include <pthread.h>
#endif

namespace ovum
{

_TlsEntry::_TlsEntry() :
	inited(false)
{ }

bool _TlsEntry::Alloc()
{
#if OVUM_TARGET == OVUM_WINDOWS
	key = TlsAlloc();
	if (key == TLS_OUT_OF_INDEXES)
		return false;
#else
	int r = pthread_key_create(&key, nullptr);
	if (r != 0)
		return false;
#endif
	this->inited = true;
	return true;
}

void _TlsEntry::Free()
{
	if (IsValid())
	{
#if OVUM_TARGET == OVUM_WINDOWS
		TlsFree(key);
#else
		pthread_key_delete(key);
#endif
		inited = false;
	}
}

void *const _TlsEntry::Get()
{
	if (!IsValid())
		return nullptr;

#if OVUM_TARGET == OVUM_WINDOWS
	return TlsGetValue(key);
#else
	return pthread_getspecific(key);
#endif
}

void _TlsEntry::Set(void *const value)
{
	if (IsValid())
	{
#if OVUM_TARGET == OVUM_WINDOWS
		TlsSetValue(key, value);
#else
		pthread_setspecific(key, value);
#endif
	}
}

} // namespace ovum