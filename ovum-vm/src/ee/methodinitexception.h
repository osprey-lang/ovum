#pragma once

#include "../vm.h"
#include "../object/member.h"
#include "../object/field.h"

namespace ovum
{

class MethodInitException : public std::exception
{
public:
	enum FailureKind
	{
		GENERAL = 0, // no extra information
		INCONSISTENT_STACK,
		INVALID_BRANCH_OFFSET,
		INSUFFICIENT_STACK_HEIGHT,
		STACK_HAS_REFS,
		INACCESSIBLE_MEMBER,
		FIELD_STATIC_MISMATCH,
		UNRESOLVED_TOKEN,
		NO_MATCHING_OVERLOAD,
		INACCESSIBLE_TYPE,
		TYPE_NOT_CONSTRUCTIBLE,
	};

	inline static MethodInitException General(
		const char *const message,
		MethodOverload *method
	)
	{
		return MethodInitException(message, method, GENERAL);
	}

	inline static MethodInitException InconsistentStack(
		const char *const message,
		MethodOverload *method,
		size_t instrIndex
	)
	{
		auto e = MethodInitException(message, method, INCONSISTENT_STACK);
		e.instrIndex = instrIndex;
		return std::move(e);
	}

	inline static MethodInitException InvalidBranchOffset(
		const char *const message,
		MethodOverload *method,
		size_t instrIndex
	)
	{
		auto e = MethodInitException(message, method, INVALID_BRANCH_OFFSET);
		e.instrIndex = instrIndex;
		return std::move(e);
	}

	inline static MethodInitException InsufficientStackHeight(
		const char *const message,
		MethodOverload *method,
		size_t instrIndex
	)
	{
		auto e = MethodInitException(message, method, INSUFFICIENT_STACK_HEIGHT);
		e.instrIndex = instrIndex;
		return std::move(e);
	}

	inline static MethodInitException StackHasRefs(
		const char *const message,
		MethodOverload *method,
		size_t instrIndex
	)
	{
		auto e = MethodInitException(message, method, STACK_HAS_REFS);
		e.instrIndex = instrIndex;
		return std::move(e);
	}

	inline static MethodInitException InaccessibleMember(
		const char *const message,
		MethodOverload *method,
		Member *member
	)
	{
		auto e = MethodInitException(message, method, INACCESSIBLE_MEMBER);
		e.member = member;
		return std::move(e);
	}

	inline static MethodInitException FieldStaticMismatch(
		const char *const message,
		MethodOverload *method,
		Field *field
	)
	{
		auto e = MethodInitException(message, method, FIELD_STATIC_MISMATCH);
		e.member = field;
		return std::move(e);
	}

	inline static MethodInitException UnresolvedToken(
		const char *const message,
		MethodOverload *method,
		uint32_t token
	)
	{
		auto e = MethodInitException(message, method, UNRESOLVED_TOKEN);
		e.token = token;
		return std::move(e);
	}

	inline static MethodInitException NoMatchingOverload(
		const char *const message,
		MethodOverload *method,
		Method *methodGroup,
		ovlocals_t argCount
	)
	{
		auto e = MethodInitException(message, method, NO_MATCHING_OVERLOAD);
		e.noOverload.methodGroup = methodGroup;
		e.noOverload.argCount = argCount;
		return std::move(e);
	}

	inline static MethodInitException InaccessibleType(
		const char *const message,
		MethodOverload *method,
		Type *type
	)
	{
		auto e = MethodInitException(message, method, INACCESSIBLE_TYPE);
		e.type = type;
		return std::move(e);
	}

	inline static MethodInitException TypeNotConstructible(
		const char *const message,
		MethodOverload *method,
		Type *type
	)
	{
		auto e = MethodInitException(message, method, TYPE_NOT_CONSTRUCTIBLE);
		e.type = type;
		return std::move(e);
	}
	
	inline FailureKind GetFailureKind() const
	{
		return kind;
	}

	inline MethodOverload *GetMethod() const
	{
		return method;
	}

	inline size_t GetInstructionIndex() const
	{
		return instrIndex;
	}

	inline Member *GetMember() const
	{
		return member;
	}

	inline Type *GetType() const
	{
		return type;
	}

	inline uint32_t GetToken() const
	{
		return token;
	}

	inline Method *GetMethodGroup() const
	{
		return noOverload.methodGroup;
	}

	inline ovlocals_t GetArgumentCount() const
	{
		return noOverload.argCount;
	}

private:
	FailureKind kind;
	MethodOverload *method;

	union {
		size_t instrIndex;
		Member *member;
		Type *type;
		uint32_t token;
		struct {
			Method *methodGroup;
			ovlocals_t argCount;
		} noOverload;
	};

	inline MethodInitException(
		const char *const message,
		MethodOverload *method,
		FailureKind kind
	) :
		exception(message),
		method(method),
		kind(kind)
	{ }
};

} // namespace ovum
