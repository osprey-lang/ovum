#pragma once

// This file includes all the internal features of the VM, which are not visible
// if you just include "ov_vm.h". These are not supposed to be visible to users
// of the VM API.
//
// You should only ever include this from within CPP files of the VM project, or
// from within other internal header files.

#ifndef VM__VM_INTERNAL_H
#define VM__VM_INTERNAL_H

#ifndef VM_EXPORTS
#error You're not supposed to include this file from outside the VM project!
#endif

class Thread;
class Type;
class Module;
class Member;
class Method;
class Field;
class Property;
class StaticRef;

class MethodInitException;

class PathName;

namespace debug
{
	class DebugSymbols;
	class ModuleDebugData;
}

typedef Thread *const ThreadHandle;
typedef Type     *TypeHandle;
typedef Module   *ModuleHandle;
typedef Member   *MemberHandle;
typedef Method   *MethodHandle;
typedef Field    *FieldHandle;
typedef Property *PropertyHandle;

#define OVUM_HANDLES_DEFINED

#include "../inc/ov_vm.h"

#if OVUM_TARGET == OVUM_WINDOWS
# include "../windows/windows.h"
#endif

#include <cstdio>

typedef uint32_t TokenId;

class VM
{
public:
	typedef struct IniterFunctions_S
	{
		ListInitializer initListInstance;
		HashInitializer initHashInstance;
		TypeTokenInitializer initTypeToken;
	} IniterFunctions;

private:
	// The main thread on which the VM is running.
	Thread *mainThread;

	// Number of command-line arguments.
	int argCount;
	// Command-line argument values.
	Value **argValues;
	// The path (sans file name) of the startup file.
	PathName *startupPath;
	// The path to the 'lib' subdirectory in the directory
	// of the startup file.
	PathName *startupPathLib;
	// The directory from which modules are loaded.
	PathName *modulePath;
	// Whether the VM describes the startup process.
	bool verbose;

	Module *startupModule;

	int LoadModules(VMStartParams &params);
	int InitArgs(int argCount, const wchar_t *args[]);

	static FILE *stdOut;
	static FILE *stdErr;

	static void PrintInternal(FILE *file, const wchar_t *format, String *str);

public:
	StandardTypes types;
	IniterFunctions functions;

	VM(VMStartParams &params);
	~VM();

	int Run();

	NOINLINE static int Init(VMStartParams &params);
	NOINLINE static void Unload();

	static void Print(String *str);
	static void Printf(const wchar_t *format, String *str);
	static void PrintLn(String *str);

	static void PrintErr(String *str);
	static void PrintfErr(const wchar_t *format, String *str);
	static void PrintErrLn(String *str);

	inline int GetArgCount() { return argCount; }
	int GetArgs(int destLength, String *dest[]);
	int GetArgValues(int destLength, Value dest[]);

	static void PrintUnhandledError(Value &error);
	static void PrintMethodInitException(MethodInitException &e);

	static VM *vm;

	friend class GC;
	friend class Module;

	friend int VM_Start(VMStartParams *params);
};

#include "string_hash.internal.h"
#include "ov_static_strings.internal.h"
#include "ov_value.internal.h"
#include "ov_type.internal.h"
#include "ov_thread.internal.h"
#include "ov_gc.internal.h"

#endif // VM__VM_INTERNAL