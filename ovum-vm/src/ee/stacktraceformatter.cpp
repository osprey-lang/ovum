#include "stacktraceformatter.h"
#include "thread.h"
#include "../object/type.h"
#include "../object/method.h"
#include "../debug/debugsymbols.h"
#include <exception>

namespace ovum
{

String *StackTraceFormatter::GetStackTrace(Thread *const thread)
{
	try
	{
		StringBuffer buf(STRING_BUFFER_CAPACITY);

		GetStackTrace(thread, buf);

		return buf.ToString(thread);
	}
	catch (std::exception&)
	{
		return nullptr;
	}
}

void StackTraceFormatter::GetStackTrace(Thread *const thread, StringBuffer &buf)
{
	const StackFrame *frame = thread->GetCurrentFrame();
	const void *ip = thread->GetInstructionPointer();

	// The VM creates a "fake" stack frame without a method, so that arguments
	// for the main method call can be pushed onto the stack. We don't want to
	// include that stack frame in the trace; it doesn't have any useful info.

	while (frame && frame->method)
	{
		AppendStackFrame(thread, buf, frame, ip);

		ip = frame->prevInstr;
		frame = frame->prevFrame;
	}
}

void StackTraceFormatter::AppendStackFrame(Thread *const thread, StringBuffer &buf, const StackFrame *frame, const void *ip)
{
	MethodOverload *method = frame->method;
	Method *group = method->group;

	// Each line in the stack trace is indented with two spaces.
	buf.Append(2, ' ');

	AppendMethodName(thread, buf, group);

	buf.Append('(');

	AppendParameters(thread, buf, frame, method);

	buf.Append(')');

	if (method->debugSymbols)
		AppendSourceLocation(thread, buf, method, ip);

	buf.Append('\n');
}

void StackTraceFormatter::AppendMethodName(Thread *const thread, StringBuffer &buf, Method *method)
{
	// The method name is the fully qualified name of the method, quite simply.
	// If method->declType is null, we're dealing with a global function, where
	// method->name already contains the fully qualified name.

	if (method->declType != nullptr)
	{
		buf.Append(method->declType->fullName);
		buf.Append('.');
	}

	buf.Append(method->name);
}

void StackTraceFormatter::AppendParameters(Thread *const thread, StringBuffer &buf, const StackFrame *frame, MethodOverload *method)
{
	ovlocals_t paramCount = method->GetEffectiveParamCount();
	const Value *args = reinterpret_cast<const Value*>(frame) - paramCount;

	for (ovlocals_t i = 0; i < paramCount; i++)
	{
		if (i > 0)
			buf.Append(2, ", ");

		if (i == 0 && method->IsInstanceMethod())
			buf.Append(4, "this");
		else
			buf.Append(method->paramNames[i - method->InstanceOffset()]);

		buf.Append('=');

		AppendArgumentType(thread, buf, args + i);
	}
}

void StackTraceFormatter::AppendArgumentType(Thread *const thread, StringBuffer &buf, const Value *arg)
{
	Value argValue; // Copy, because arg is const

	if (IS_REFERENCE(*arg))
	{
		// If the argument is a reference, it must be dereferenced before
		// we can make use of the type information.
		buf.Append(4, "ref ");
		ReadReference(const_cast<Value *>(arg), &argValue);
	}
	else
	{
		argValue = *arg;
	}

	Type *type = argValue.type;
	if (type == nullptr)
	{
		buf.Append(4, "null");
		return;
	}

	buf.Append(type->fullName);

	// When the argument is an aves.Method, we append some information about
	// the instance and method group, too, in the format
	//   aves.Method(this=<instance type>, <method name>)
	//
	// Note that this is applied recursively to the instance type, which means
	// you can end up with situations like
	//   aves.Method(this=aves.Method(this=aves.Method(...), ...), ...)
	//
	// However, it should be impossible for an aves.Method to be bound to iself.
	// Otherwise, well, we'll get infinite recursion!
	if (type == thread->GetVM()->types.Method)
	{
		// Append some information about the instance and method group, too.
		MethodInst *method = argValue.v.method;

		buf.Append(6, "(this=");

		// It should be impossible for an aves.Method to be bound to iself.
		OVUM_ASSERT(method->instance.v.instance != argValue.v.instance);
		AppendArgumentType(thread, buf, &method->instance);

		buf.Append(2, ", ");

		AppendMethodName(thread, buf, method->method);

		buf.Append(')');
	}
}

void StackTraceFormatter::AppendSourceLocation(Thread *const thread, StringBuffer &buf, MethodOverload *method, const void *ip)
{
	uint32_t offset = (uint32_t)((uint8_t*)ip - method->entry);

	debug::DebugSymbol *sym = method->debugSymbols->FindSymbol(offset);
	if (sym == nullptr)
		return;

	buf.Append(9, " at line ");
	AppendLineNumber(thread, buf, sym->startLocation.lineNumber);

	buf.Append(5, " in \"");
	buf.Append(sym->file->fileName);
	buf.Append('"');
}

void StackTraceFormatter::AppendLineNumber(Thread *const thread, StringBuffer &buf, int32_t line)
{
	// 32 digits ought to be enough for anybody
	const ovchar_t BUFFER_SIZE = 32;

	ovchar_t lineNumberStr[BUFFER_SIZE];
	ovchar_t *chp = lineNumberStr + BUFFER_SIZE;
	int32_t length = 0;

	do
	{
		*--chp = (ovchar_t)'0' + line % 10;
		length++;
	} while (line /= 10);

	buf.Append(length, chp);
}

} // namespace ovum
