#pragma once

#include "../vm.h"
#include "../util/stringbuffer.h"

namespace ovum
{

// Formats stack traces for a managed thread. Stack traces contain information
// about (managed) method calls, including:
//  * The fully qualified name of the method;
//  * The argument types and parameter names;
//  * The type of the instance ('this' type), if it is an instance method;
//  * Whether an argument is passed by reference or value; and
//  * The source location, if debug information is available.
//
// The stack trace is usually returned as a String*, so that it can be passed to
// client code without the need to convert, but can also be written directly to
// a StringBuffer.
class StackTraceFormatter
{
	// Note: this class only has static members so far.

public:
	// Returns a new String* containing a stack trace of the thread's current state.
	static String *GetStackTrace(Thread *const thread);

	// Appends a stack trace of the thread's current state to the specified string buffer.
	static void GetStackTrace(Thread *const thread, StringBuffer &buf);

private:
	static void AppendStackFrame(Thread *const thread, StringBuffer &buf, const StackFrame *frame, const void *ip);

	static void AppendMethodName(Thread *const thread, StringBuffer &buf, Method *method);

	static void AppendParameters(Thread *const thread, StringBuffer &buf, const StackFrame *frame, MethodOverload *method);

	static void AppendArgumentType(Thread *const thread, StringBuffer &buf, const Value *arg);

	static void AppendShortMemberName(Thread *const thread, StringBuffer &buf, String *fullName);

	static void AppendShortMethodName(Thread *const thread, StringBuffer &buf, Method *method);

	static void AppendSourceLocation(Thread *const thread, StringBuffer &buf, MethodOverload *method, const void *ip);

	static void AppendLineNumber(Thread *const thread, StringBuffer &buf, int32_t line);

	static const size_t STRING_BUFFER_CAPACITY = 1024;
};

} // namespace ovum
