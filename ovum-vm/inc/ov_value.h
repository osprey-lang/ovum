#pragma once

#ifndef VM__VALUE_H
#define VM__VALUE_H

#include <cstdint>
#include "ov_compat.h"


// All Osprey strings are UTF-16, guaranteed.
#if OVUM_WCHAR_SIZE == 2
// If sizeof(wchar_t) is 2, we assume it's UTF-16, and
// define uchar to be of that type.
typedef wchar_t uchar;
#else
// Otherwise, we have to fall back to uint16_t.
typedef uint16_t uchar;
#endif

enum class StringFlags : uint32_t
{
	NONE = 0,
	// Tells the GC not to collect a String*, because it was created
	// from some static resource.
	STATIC = 1,
	// The string has been hashed (its hashCode contains a usable value).
	// This should ONLY be set by String_GetHashCode! If you set this flag
	// anywhere else, you will break the VM. Probably. It's C++. Who knows.
	HASHED = 2,
	// The string is interned. This flag is only used by the GC, to determine
	// whether the string needs to be removed from the intern table when it
	// is collected.
	INTERN = 4,
};
ENUM_OPS(StringFlags, uint32_t);

// Note: Strings are variable-size instances, and should never be passed by value.
// Always pass String pointers around. To get the character data, take the address
// of the firstChar field.
typedef struct String_S
{
	// The length of the string, not including the terminating \0.
	const int32_t length;
	// The string's hash code. If the string has had its hash code calculated (if the
	// flag StringFlags::HASHED is set), then this field contains the hash code of the
	// string. Otherwise, this value is nonsensical.
	int32_t hashCode;
	// If the flags contain StringFlags::STATIC, the String is never garbage collected,
	// as it comes from a static resource.
	// If the flags contain StringFlags::HASHED, then hashCode contains the string's
	// hash code. Otherwise, don't rely on it.
	StringFlags flags;
	// The first character.
	const uchar firstChar;
} String;


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
	uchar chars[Len + 1];

	typedef const char CString[Len + 1];

	static inline LitString<Len> FromCString(CString data)
	{
		LitString<Len> output = { Len, 0, StringFlags::STATIC };

		// Note: we can't use memcpy or CopyMemoryT because sizeof(char) != sizeof(uchar).
		for (int i = 0; i < Len; i++)
			output.chars[i] = data[i];

		return output;
	}
};

// Reinterpret-casts a LitString<> to a String*.
// NOTE: do not pass a LitString<> pointer into this!
#define _S(ls)	reinterpret_cast<::String*>(&ls)

// forward declarations
typedef struct ListInst_S ListInst;
typedef struct HashInst_S HashInst;
typedef struct ErrorInst_S ErrorInst;
typedef struct MethodInst_S MethodInst;

// Represents a single value. If the value is of a primtive type
// (that is, (type->flags & TYPE_PRIMITIVE) != TYPE_NONE), then
// the integer, uinteger or real fields contain the instance data.
// Otherwise, the instance field contains a pointer to the instance.
// If the value is a reference (IS_REFERENCE(value)), then the
// reference field points to the referent's storage location.
typedef struct Value_S Value;
struct Value_S
{
	TypeHandle type;

	union
	{
		int64_t integer;
		uint64_t uinteger;
		double real;

		// The instance is just a pointer to some bytes.
		uint8_t *instance;
		union
		{
			String *string;
			ListInst *list;
			HashInst *hash;
			ErrorInst *error;
			MethodInst *method;
		} common;
		void *reference;
	};
};

#define IS_REFERENCE(value) (((uintptr_t)(value).type & 1) == 1)


struct ListInst_S
{
	int32_t capacity; // the length of 'values'
	int32_t length;   // the actual number of items contained in the list
	int32_t version;  // the "version" of the list, which is incremented each time the list changes
	Value *values;    // the values contained in the list
};


typedef struct HashEntry_S
{
	int32_t hashCode; // Lower 31 bits of hash code; -1 = unused
	int32_t next;     // Index of next entry in bucket; -1 = last
	Value key;
	Value value;
} HashEntry;
struct HashInst_S
{
	int32_t capacity;   // the number of "slots" in buckets and entries
	int32_t count;      // the number of entries (not buckets) that have been used
	int32_t freeCount;  // the number of entries that were previously used, and have now been freed (and can thus be reused)
	int32_t freeList;   // the index of the first freed entry
	int32_t version;    // the "version" of the hash, incremented whenever changes are made

	int32_t *buckets;
	HashEntry *entries;
};

struct ErrorInst_S
{
	String *message;
	String *stackTrace;
	Value innerError;
	Value data;
};


#include "ov_type.h"


struct MethodInst_S
{
	Value instance;
	MethodHandle method;
};


#define NULL_CONSTANT  { nullptr, 0 }
OVUM_API extern const Value NULL_VALUE;

inline void SetNull(Value &target)
{
	target.type = nullptr;
}
inline void SetNull(Value *target)
{
	target->type = nullptr;
}

inline void SetBool(ThreadHandle thread, Value &target, bool value)
{
	target.type = GetType_Boolean(thread);
	target.integer = value;
}
inline void SetBool(ThreadHandle thread, Value *target, bool value)
{
	target->type = GetType_Boolean(thread);
	target->integer = value;
}

inline void SetInt(ThreadHandle thread, Value &target, int64_t value)
{
	target.type = GetType_Int(thread);
	target.integer = value;
}
inline void SetInt(ThreadHandle thread, Value *target, int64_t value)
{
	target->type = GetType_Int(thread);
	target->integer = value;
}

inline void SetUInt(ThreadHandle thread, Value &target, uint64_t value)
{
	target.type = GetType_UInt(thread);
	target.uinteger = value;
}
inline void SetUInt(ThreadHandle thread, Value *target, uint64_t value)
{
	target->type = GetType_UInt(thread);
	target->uinteger = value;
}

inline void SetReal(ThreadHandle thread, Value &target, double value)
{
	target.type = GetType_Real(thread);
	target.real = value;
}
inline void SetReal(ThreadHandle thread, Value *target, double value)
{
	target->type = GetType_Real(thread);
	target->real = value;
}

inline void SetString(ThreadHandle thread, Value &target, String *value)
{
	target.type = GetType_String(thread);
	target.common.string = value;
}
inline void SetString(ThreadHandle thread, Value *target, String *value)
{
	target->type = GetType_String(thread);
	target->common.string = value;
}


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
		return reinterpret_cast<T*>(value->instance);
	}
	inline T *operator*() const
	{
		return reinterpret_cast<T*>(value->instance);
	}

	inline TypeHandle GetType() const
	{
		return value->type;
	}
};

#endif // VM__VALUE_H