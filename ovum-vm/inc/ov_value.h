#ifndef VM__VALUE_H
#define VM__VALUE_H

#include "ov_vm.h"

// This aggregate template class allows a String to be represented "literally",
// in an aggregate initializer. Remember that you have to repeat the length twice.
// And then you can safely cast LitString* to String*!
template<int Len>
class LitString
{
public:
	const int32_t length;
	int32_t hashCode;
	StringFlags flags;
	ovchar_t chars[Len + 1];

	typedef const char CString[Len + 1];

	// Reinterprets the LitString<> as a String*, by casting the 'this' pointer.
	// If the LitString<> goes out of scope, the String* will be invalidated.
	// Note: this method cannot be declared const, because String* is required
	// to be mutable: the VM must be able to update the 'flags' field.
	inline String *AsString()
	{
		return reinterpret_cast<String*>(this);
	}

	static inline LitString<Len> FromCString(CString data)
	{
		LitString<Len> output = { Len, 0, StringFlags::STATIC };

		// Note: we can't use memcpy or CopyMemoryT because sizeof(char) != sizeof(ovchar_t).
		for (int i = 0; i < Len; i++)
			output.chars[i] = data[i];

		return output;
	}
};

#define IS_PRIMITIVE(value) ((Type_GetFlags((value).type) & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE)
#define IS_REFERENCE(value) (((uintptr_t)(value).type & 1) == 1)

#define NULL_CONSTANT  { nullptr, 0 }
OVUM_API extern const Value NULL_VALUE;

#define IS_NULL(v)		((v).type == nullptr)

OVUM_API bool IsTrue(Value *value);

OVUM_API bool IsFalse(Value *value);

OVUM_API bool IsType(Value *value, TypeHandle type);

OVUM_API bool IsSameReference(Value *a, Value *b);

OVUM_API bool IsBoolean(ThreadHandle thread, Value *value);

OVUM_API bool IsInt(ThreadHandle thread, Value *value);

OVUM_API bool IsUInt(ThreadHandle thread, Value *value);

OVUM_API bool IsReal(ThreadHandle thread, Value *value);

OVUM_API bool IsString(ThreadHandle thread, Value *value);

OVUM_API void ReadReference(Value *ref, Value *target);

OVUM_API void WriteReference(Value *ref, Value *value);

template<typename T>
class Alias
{
private:
	Value *value;

public:
	inline Alias(Value *value) : value(value) { }

	inline T *operator->() const
	{
		return reinterpret_cast<T*>(value->v.instance);
	}
	inline T *operator*() const
	{
		return reinterpret_cast<T*>(value->v.instance);
	}

	inline TypeHandle GetType() const
	{
		return value->type;
	}
};

#endif // VM__VALUE_H
