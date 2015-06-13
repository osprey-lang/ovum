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

namespace ovum
{

class Field;
class GC;
class GCObject;
class Member;
class Method;
class MethodInitException;
class MethodInitializer;
class MethodOverload;
class Module;
class ModulePool;
class ModuleReader;
class PathName;
class Property;
class RefSignaturePool;
class StackManager;
class StaticRef;
class StaticRefBlock;
class Thread;
class TryBlock;
class Type;
class VM;
struct CatchBlock;
struct CatchBlocks;
struct FinallyBlock;

namespace debug
{
	class DebugSymbols;
	class ModuleDebugData;
}

namespace instr
{
	class Instruction;
	class MethodBuffer;
	class MethodBuilder;
}

} // namespace ovum

typedef ovum::Thread *const ThreadHandle;
typedef ovum::Type           *TypeHandle;
typedef ovum::Module         *ModuleHandle;
typedef ovum::Member         *MemberHandle;
typedef ovum::Method         *MethodHandle;
typedef ovum::MethodOverload *OverloadHandle;
typedef ovum::Field          *FieldHandle;
typedef ovum::Property       *PropertyHandle;

#define OVUM_HANDLES_DEFINED

#include "../inc/ov_vm.h"

#if OVUM_WINDOWS
# if !defined(UNICODE) || !defined(_UNICODE)
#  error Ovum on Windows must be compiled with Unicode support; make sure UNICODE and _UNICODE are defined.
# endif
# include "os/windows.h"
#endif

#include <cstdio>
#include "threading/tls.h"

namespace ovum
{

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

	GC *gc;
	ModulePool *modules;
	RefSignaturePool *refSignatures;

	int LoadModules(VMStartParams &params);
	int InitArgs(int argCount, const wchar_t *args[]);
	int GetMainMethodOverload(Method *method, unsigned int &argc, MethodOverload *&overload);

	static void PrintInternal(FILE *file, const wchar_t *format, String *str);

public:
	StandardTypes types;
	IniterFunctions functions;

	VM(VMStartParams &params);
	~VM();

	int Run();

	NOINLINE static int Create(VMStartParams &params, VM *&result);

	inline GC *GetGC() const
	{
		return gc;
	}
	inline ModulePool *GetModulePool() const
	{
		return modules;
	}
	inline RefSignaturePool *GetRefSignaturePool() const
	{
		return refSignatures;
	}

	static void Print(String *str);
	static void Printf(const wchar_t *format, String *str);
	static void PrintLn(String *str);

	static void PrintErr(String *str);
	static void PrintfErr(const wchar_t *format, String *str);
	static void PrintErrLn(String *str);

	inline int GetArgCount() { return argCount; }
	int GetArgs(int destLength, String *dest[]);
	int GetArgValues(int destLength, Value dest[]);

	void PrintUnhandledError(Thread *const thread);
	void PrintMethodInitException(MethodInitException &e);

	// Contains the VM running on the current thread.
	static TlsEntry<VM> vmKey;

	friend class GC;
	friend class Module;

	friend int ::VM_Start(VMStartParams *params);
};

} // namespace ovum

#include "res/staticstrings.h"

#endif // VM__VM_INTERNAL