#include "standardtypeinfo.h"
#include "../ee/vm.h"
#include "../res/staticstrings.h"
#include <new>

namespace ovum
{

const int StandardTypeCollection::STANDARD_TYPE_COUNT = 19;

int StandardTypeCollection::Create(VM *vm, StandardTypeCollection *&result)
{
	StandardTypeCollection *output = new(std::nothrow) StandardTypeCollection();
	if (output == nullptr)
		return OVUM_ERROR_NO_MEMORY;

	output->InitTypeInfo(vm);

	result = output;
	RETURN_SUCCESS;
}

StandardTypeCollection::StandardTypeCollection() :
	types(STANDARD_TYPE_COUNT)
{ }

StandardTypeCollection::~StandardTypeCollection()
{
	// Nothing really to do here!
}

void StandardTypeCollection::InitTypeInfo(VM *vm)
{
	using T = ::StandardTypes;
	
	StaticStrings::typesStrings &t = vm->GetStrings()->types;

	Add(t.aves.Object,              &T::Object,              nullptr);
	Add(t.aves.Boolean,             &T::Boolean,             nullptr);
	Add(t.aves.Int,                 &T::Int,                 nullptr);
	Add(t.aves.UInt,                &T::UInt,                nullptr);
	Add(t.aves.Real,                &T::Real,                nullptr);
	Add(t.aves.String,              &T::String,              nullptr);
	Add(t.aves.List,                &T::List,                "InitListInstance");
	Add(t.aves.Hash,                &T::Hash,                "InitHashInstance");
	Add(t.aves.Method,              &T::Method,              nullptr);
	Add(t.aves.Iterator,            &T::Iterator,            nullptr);
	Add(t.aves.Error,               &T::Error,               nullptr);
	Add(t.aves.TypeError,           &T::TypeError,           nullptr);
	Add(t.aves.MemoryError,         &T::MemoryError,         nullptr);
	Add(t.aves.OverflowError,       &T::OverflowError,       nullptr);
	Add(t.aves.NoOverloadError,     &T::NoOverloadError,     nullptr);
	Add(t.aves.DivideByZeroError,   &T::DivideByZeroError,   nullptr);
	Add(t.aves.NullReferenceError,  &T::NullReferenceError,  nullptr);
	Add(t.aves.MemberNotFoundError, &T::MemberNotFoundError, nullptr);
	Add(t.aves.reflection.Type,     &T::Type,                "InitTypeToken");
}

inline void StandardTypeCollection::Add(String *name, TypeHandle StandardTypes::*member, const char *initerFunction)
{
	types.Add(name, StandardTypeInfo(name, member, initerFunction));
}

} // namespace ovum
