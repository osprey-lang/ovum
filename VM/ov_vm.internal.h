#pragma once

#ifndef VM__VM_INTERNAL_H
#define VM__VM_INTERNAL_H

#ifndef VM_EXPORTS
#error You're not supposed to include this file from outside the VM project!
#endif

// This file includes all the internal features of the VM, which are not visible
// if you just include "vm.h". These are not /supposed/ to be visible to users
// of the VM DLL API.
//
// You should probably only include this from within CPP files of the VM project,
// or from within other internal header files.

#include "ov_vm.h"

// So that all the VM functions can access this field easily.
extern StandardTypes stdTypes; // defined in vm.cpp

// Used heavily throughout!

// Recovers a Type from a TypeHandle.
// (Note: the name _Ty clashes with various things in the Windows headers. Don't use it.)
#define _Tp(th)		reinterpret_cast<const ::Type*>(th)

typedef struct GlobalFunctions_S
{
	ListInitializer initListInstance;
	HashInitializer initHashInstance;
	TypeTokenInitializer initTypeToken;
} GlobalFunctions;
extern GlobalFunctions globalFunctions; // defined in vm.cpp

#include "string_hash.internal.h"
#include "ov_static_strings.internal.h"
#include "ov_value.internal.h"
#include "ov_type.internal.h"
#include "ov_thread.internal.h"
#include "ov_thread.opcodes.h"
#include "ov_gc.internal.h"
#include "ov_module.internal.h"

typedef struct VMState_S
{
	// The main thread on which the VM is running.
	// NOTE: this is not declared Thread*const because we need to be able
	// to assign to it when initialising the VM.
	Thread *mainThread;
	// Number of command-line arguments.
	int argCount;
	// Command-line argument values.
	String **argValues;
	// The path (sans file name) of the startup file.
	String *startupPath;
	// The directory from which modules are loaded.
	String *modulePath;
	// Whether the VM describes the startup process.
	bool verbose;
} VMState;

extern VMState vmState;

void VM_Init(VMStartParams params);

#endif // VM__VM_INTERNAL