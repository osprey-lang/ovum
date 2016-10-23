#include "standardtypeinfo.h"
#include "standardtypeiniters.h"
#include "../ee/vm.h"
#include "../res/staticstrings.h"

namespace ovum
{

const int StandardTypeCollection::STANDARD_TYPE_COUNT = 20;

Box<StandardTypeCollection> &&StandardTypeCollection::New(VM *vm)
{
	Box<StandardTypeCollection> output(new(std::nothrow) StandardTypeCollection());
	if (!output)
		return nullptr;

	if (!output->Init(vm))
		return nullptr;

	return std::move(output);
}

StandardTypeCollection::StandardTypeCollection() :
	types(STANDARD_TYPE_COUNT)
{ }

bool StandardTypeCollection::Init(VM *vm)
{
	using T = ::StandardTypes;
	
	auto &t = vm->GetStrings()->types;

	Add(t.aves.Object,              &T::Object,              StandardTypeIniters::InitObjectType);
	Add(t.aves.Boolean,             &T::Boolean,             nullptr);
	Add(t.aves.Int,                 &T::Int,                 nullptr);
	Add(t.aves.UInt,                &T::UInt,                nullptr);
	Add(t.aves.Real,                &T::Real,                nullptr);
	Add(t.aves.String,              &T::String,              nullptr);
	Add(t.aves.List,                &T::List,                StandardTypeIniters::InitListType);
	Add(t.aves.Hash,                &T::Hash,                StandardTypeIniters::InitHashType);
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
	Add(t.aves.TypeConversionError, &T::TypeConversionError, nullptr);
	Add(t.aves.reflection.Type,     &T::Type,                StandardTypeIniters::InitTypeType);

	return true;
}

inline void StandardTypeCollection::Add(String *name, TypeHandle StandardTypes::*member, StandardTypeIniter extendedIniter)
{
	types.Add(name, StandardTypeInfo(name, member, extendedIniter));
}

} // namespace ovum
