#include "../vm.h"
#include "methodinitializer.h"
#include "methodbuilder.h"

namespace ovum
{

int Thread::InitializeMethod(MethodOverload *method)
{
	assert(!method->IsInitialized());

	MethodInitializer initer(this->vm);
	int r = initer.Initialize(method, this);
	return r;
}

int Thread::CallStaticConstructors(instr::MethodBuilder &builder)
{
	for (int32_t i = 0; i < builder.GetTypeCount(); i++)
	{
		Type *type = builder.GetType(i);
		// The static constructor may have been triggered by a previous type initialization,
		// so we must test the flag again
		int r = type->RunStaticCtor(this);
		if (r != OVUM_SUCCESS)
			return r;
	}
	RETURN_SUCCESS;
}

} // namespace ovum