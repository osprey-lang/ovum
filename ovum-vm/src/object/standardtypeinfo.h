#pragma once

#include "../vm.h"
#include "../util/stringhash.h"

namespace ovum
{

// Performs additional initialization or verification of a standard type.
//
// Parameters:
//   vm:
//     The VM instance to which the type belongs.
//   declModule:
//     The module in which the type is declared.
//   type:
//     The type that requires additional initialization.
// Returns:
//   An Ovum status code. If no error occurs, OVUM_SUCCESS is returned.
//   A StandardTypeIniter can also indicate errors by throwing a ModuleLoadException.
typedef int (*StandardTypeIniter)(VM *vm, Module *declModule, Type *type);

struct StandardTypeInfo
{
	// The name of the type.
	String *name;
	// The StandardTypes member that holds the instance of this type.
	TypeHandle StandardTypes::*member;
	// If not null, holds the address of an extended initializer function, which
	// is used to perform additional initialization or verification of the type.
	StandardTypeIniter extendedIniter;

	inline StandardTypeInfo()
	{
		// Don't initialize anything.
	}
	inline StandardTypeInfo(const StandardTypeInfo &other) :
		name(other.name),
		member(other.member),
		extendedIniter(other.extendedIniter)
	{ }
	inline StandardTypeInfo(String *name, TypeHandle StandardTypes::*member, StandardTypeIniter extendedIniter) :
		name(name),
		member(member),
		extendedIniter(extendedIniter)
	{ }
};

// Holds information about the "standard types" - that is, fundamental types that
// are required for the VM to function properly. This includes Object, String, Int,
// UInt, Real, Error, List, Hash, and several others.
// This class is used during module loading, to find out whether a type is indeed
// a standard type (based on its name), and, if so, which StandardTypes field it
// should be assigned to. Some types also have special initializer functions that
// must be exported by the native module declaring the type.
class StandardTypeCollection
{
private:
	StringHash<StandardTypeInfo> types;

public:
	OVUM_NOINLINE static Box<StandardTypeCollection> New(VM *vm);
	~StandardTypeCollection();

	inline int32_t GetCount() const
	{
		return types.GetCount();
	}

	inline bool Get(String *name, StandardTypeInfo &result) const
	{
		return types.Get(name, result);
	}

	inline bool GetByIndex(int32_t index, StandardTypeInfo &result) const
	{
		return types.GetByIndex(index, result);
	}

private:
	StandardTypeCollection();

	bool Init(VM *vm);

	void Add(String *name, TypeHandle StandardTypes::*member, StandardTypeIniter extendedIniter);

	static const int STANDARD_TYPE_COUNT;
};

} // namespace ovum
