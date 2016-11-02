#pragma once

#include "../vm.h"
#include "../../inc/ovum_main.h"
#include "../threading/tls.h"
#include <cstdio>

namespace ovum
{

class VM
{
public:
	struct StandardNativeFunctions
	{
		ListInitializer initListInstance;
		HashInitializer initHashInstance;
		TypeTokenInitializer initTypeToken;
	};

private:
	// The main thread on which the VM is running.
	Box<Thread> mainThread;

	// Number of command-line arguments.
	int argCount;
	// Command-line argument values. Each Value* is a pointer into
	// a StaticRef.
	Box<Value*[]> argValues;

	// The path (sans file name) of the startup file.
	Box<PathName> startupPath;
	// The path to the 'lib' subdirectory in the directory
	// of the startup file.
	Box<PathName> startupPathLib;
	// The directory from which modules are loaded.
	Box<PathName> modulePath;

	// Whether the VM describes the startup process.
	bool verbose;

	Module *startupModule;

	// The current garbage collector.
	Box<GC> gc;

	// The module pool, which contains all currently loaded modules.
	Box<ModulePool> modules;

	// The reference signature pool. See RefSignature for more details.
	Box<RefSignaturePool> refSignatures;

	// Standard types which the VM requires in order to operate, such as
	// aves.Int, aves.String, aves.Error and the like.
	Box<StandardTypeCollection> standardTypeCollection;

	// Static strings, mostly member names and error messages.
	Box<StaticStrings> strings;

	int LoadModules(VMStartParams &params);
	int InitArgs(int argCount, const wchar_t *args[]);
	int GetMainMethodOverload(Method *method, ovlocals_t &argc, MethodOverload *&overload);

	static void PrintInternal(FILE *file, const wchar_t *format, String *str);

public:
	StandardTypes types;
	StandardNativeFunctions functions;

	VM(VMStartParams &params);
	~VM();

	int Run();

	OVUM_NOINLINE static int New(VMStartParams &params, Box<VM> &result);

	inline GC *GetGC() const
	{
		return gc.get();
	}

	inline ModulePool *GetModulePool() const
	{
		return modules.get();
	}

	inline RefSignaturePool *GetRefSignaturePool() const
	{
		return refSignatures.get();
	}

	inline StaticStrings *GetStrings() const
	{
		return strings.get();
	}

	inline StandardTypeCollection *GetStandardTypeCollection() const
	{
		return standardTypeCollection.get();
	}

	inline const PathName *GetStartupPathLib() const
	{
		return startupPathLib.get();
	}

	inline const PathName *GetStartupPath() const
	{
		return startupPath.get();
	}

	inline const PathName *GetModulePath() const
	{
		return modulePath.get();
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
	friend class RootSetWalker;

	friend int ::VM_Start(VMStartParams *params);
};

} // namespace ovum
