#pragma once

#ifndef VM__VM_INTERNAL_H
#define VM__VM_INTERNAL_H

//#define PRINT_DEBUG_INFO

#if defined(_WIN32) && !defined(UNICODE)
#error You are not supposed to compile Ovum without Unicode support.
#endif

#ifndef VM_EXPORTS
#error You're not supposed to include this file from outside the VM project!
#endif

// This file includes all the internal features of the VM, which are not visible
// if you just include "ov_vm.h". These are not /supposed/ to be visible to users
// of the VM DLL API.
//
// You should probably only include this from within CPP files of the VM project,
// or from within other internal header files.

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows header files
#include <windows.h>

class Thread;
class Type;
class Module;
class Member;
class Method;
class Field;
class Property;
class StaticRef;

class MethodInitException;

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

#include "ov_vm.h"
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
	String *startupPath;
	// The directory from which modules are loaded.
	String *modulePath;
	// Whether the VM describes the startup process.
	bool verbose;

	Module *startupModule;

	void LoadModules(VMStartParams &params);
	void InitArgs(int argCount, const wchar_t *args[]);

	static FILE *stdOut;
	static FILE *stdErr;

	static void PrintInternal(FILE *file, const wchar_t *format, String *str);

public:
	StandardTypes types;
	IniterFunctions functions;

	VM(VMStartParams &params);
	~VM();

	int Run(VMStartParams &params);

	NOINLINE static void Init(VMStartParams &params);
	NOINLINE static void Unload();

	static void Print(String *str);
	static void Printf(const wchar_t *format, String *str);
	static void PrintLn(String *str);

	static void PrintErr(String *str);
	static void PrintfErr(const wchar_t *format, String *str);
	static void PrintErrLn(String *str);

	inline int GetArgCount() { return argCount; }
	int GetArgs(const int destLength, String *dest[]);
	int GetArgValues(const int destLength, Value dest[]);

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