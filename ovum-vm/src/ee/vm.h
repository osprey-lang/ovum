#pragma once

#include "../vm.h"
#include "../../inc/ov_main.h"
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
	StandardTypeCollection *standardTypeCollection;
	StaticStrings *strings;

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

	OVUM_NOINLINE static int Create(VMStartParams &params, VM *&result);

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

	inline StaticStrings *GetStrings() const
	{
		return strings;
	}

	inline StandardTypeCollection *GetStandardTypeCollection() const
	{
		return standardTypeCollection;
	}

	inline const PathName *GetStartupPathLib() const
	{
		return startupPathLib;
	}

	inline const PathName *GetStartupPath() const
	{
		return startupPath;
	}

	inline const PathName *GetModulePath() const
	{
		return modulePath;
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
