#ifndef VM__STANDARDTYPEINFO_H
#define VM__STANDARDTYPEINFO_H

#include "../vm.h"
#include "../util/stringhash.h"

namespace ovum
{

struct StandardTypeInfo
{
	// The name of the type.
	String *name;
	// The StandardTypes member that holds the instance of this type.
	TypeHandle StandardTypes::*member;
	// If not null, holds the name of an initializer function, which is used by
	// the VM to construct and instance of the type. Only some types have such
	// a function.
	const char *initerFunction;

	inline StandardTypeInfo()
	{
		// Don't initialize anything.
	}
	inline StandardTypeInfo(const StandardTypeInfo &other) :
		name(other.name),
		member(other.member),
		initerFunction(other.initerFunction)
	{ }
	inline StandardTypeInfo(String *name, TypeHandle StandardTypes::*member, const char *const initerFunction) :
		name(name),
		member(member),
		initerFunction(initerFunction)
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
	OVUM_NOINLINE static int Create(VM *vm, StandardTypeCollection *&result);
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

	void InitTypeInfo(VM *vm);

	void Add(String *name, TypeHandle StandardTypes::*member, const char *initerFunction);

	static const int STANDARD_TYPE_COUNT;
};

} // namespace ovum

#endif // VM__STANDARDTYPEINFO_H