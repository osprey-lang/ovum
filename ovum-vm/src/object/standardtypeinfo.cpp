#include "standardtypeinfo.h"
#include "standardtypeiniters.h"
#include "../ee/vm.h"
#include "../res/staticstrings.h"

namespace ovum
{

const int StandardTypeCollection::STANDARD_TYPE_COUNT = 20;

Box<StandardTypeCollection> StandardTypeCollection::New(VM *vm)
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
	using Id = ovum::SpecialTypeId;
	using Init = ovum::StandardTypeIniters;
	
	auto &t = vm->GetStrings()->types;

	Add(t.aves.Object,              &T::Object,              Id::OBJECT,  Init::InitObjectType);
	Add(t.aves.Boolean,             &T::Boolean,             Id::BOOLEAN, nullptr);
	Add(t.aves.Int,                 &T::Int,                 Id::INT,     nullptr);
	Add(t.aves.UInt,                &T::UInt,                Id::UINT,    nullptr);
	Add(t.aves.Real,                &T::Real,                Id::REAL,    nullptr);
	Add(t.aves.String,              &T::String,              Id::STRING,  nullptr);
	Add(t.aves.List,                &T::List,                Id::NONE,    Init::InitListType);
	Add(t.aves.Hash,                &T::Hash,                Id::NONE,    Init::InitHashType);
	Add(t.aves.Method,              &T::Method,              Id::NONE,    nullptr);
	Add(t.aves.Iterator,            &T::Iterator,            Id::NONE,    nullptr);
	Add(t.aves.Error,               &T::Error,               Id::NONE,    nullptr);
	Add(t.aves.TypeError,           &T::TypeError,           Id::NONE,    nullptr);
	Add(t.aves.MemoryError,         &T::MemoryError,         Id::NONE,    nullptr);
	Add(t.aves.OverflowError,       &T::OverflowError,       Id::NONE,    nullptr);
	Add(t.aves.NoOverloadError,     &T::NoOverloadError,     Id::NONE,    nullptr);
	Add(t.aves.DivideByZeroError,   &T::DivideByZeroError,   Id::NONE,    nullptr);
	Add(t.aves.NullReferenceError,  &T::NullReferenceError,  Id::NONE,    nullptr);
	Add(t.aves.MemberNotFoundError, &T::MemberNotFoundError, Id::NONE,    nullptr);
	Add(t.aves.TypeConversionError, &T::TypeConversionError, Id::NONE,    nullptr);
	Add(t.aves.reflection.Type,     &T::Type,                Id::NONE,    Init::InitTypeType);

	return true;
}

inline void StandardTypeCollection::Add(
	String *name,
	TypeHandle StandardTypes::*member,
	SpecialTypeId specialType,
	StandardTypeIniter extendedIniter
)
{
	types.Add(name, StandardTypeInfo(name, member, specialType, extendedIniter));
}

} // namespace ovum
