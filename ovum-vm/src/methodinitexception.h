#pragma once

#ifndef VM__METHODINITEXCEPTION_INTERNAL_H
#define VM__METHODINITEXCEPTION_INTERNAL_H

#include "vm.h"

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
		UNRESOLVED_TOKEN_ID,
		NO_MATCHING_OVERLOAD,
		INACCESSIBLE_TYPE,
		TYPE_NOT_CONSTRUCTIBLE,
	};

private:
	FailureKind kind;
	MethodOverload *method;

	union {
		int32_t instrIndex;
		Member *member;
		Type *type;
		uint32_t tokenId;
		struct {
			Method *methodGroup;
			uint32_t argCount;
		} noOverload;
	};

public:
	inline MethodInitException(const char *const message, MethodOverload *method) :
		exception(message), method(method), kind(GENERAL)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, int32_t instrIndex, FailureKind kind) :
		exception(message), method(method), kind(kind), instrIndex(instrIndex)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, Member *member, FailureKind kind) :
		exception(message), method(method), kind(kind), member(member)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, Type *type, FailureKind kind) :
		exception(message), method(method), kind(kind), type(type)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, uint32_t tokenId, FailureKind kind) :
		exception(message), method(method), kind(kind), tokenId(tokenId)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method,
		Method *methodGroup, uint32_t argCount, FailureKind kind) :
		exception(message), method(method), kind(kind)
	{
		noOverload.methodGroup = methodGroup;
		noOverload.argCount = argCount;
	}

	inline FailureKind GetFailureKind() const { return kind; }

	inline MethodOverload *GetMethod() const { return method; }

	inline int32_t GetInstructionIndex() const { return instrIndex; }
	inline Member *GetMember() const { return member; }
	inline Type *GetType() const { return type; }
	inline uint32_t GetTokenId() const { return tokenId; }
	inline Method *GetMethodGroup() const { return noOverload.methodGroup; }
	inline uint32_t GetArgumentCount() const { return noOverload.argCount; }
};

}

#endif //VM__METHODINITEXCEPTION_INTERNAL_H