#pragma once

// This file includes all the internal features of the VM, which are not visible
// if you just include "ov_vm.h". These are not supposed to be visible to users
// of the VM API.
//
// You should only ever include this from within CPP files of the VM project, or
// from within other internal header files.

#ifndef VM_EXPORTS
#error You're not supposed to include this file from outside the VM project!
#endif

// Forward declarations of fundamental VM classes and structs.

namespace ovum
{

class Field;
class GC;
class GCObject;
class GlobalMember;
class LiveObjectFinder;
class Member;
class Method;
class MethodInitException;
class MethodInitializer;
class MethodOverload;
class Module;
class ModulePool;
class ModuleReader;
class MovedObjectUpdater;
template<class Visitor>
class ObjectGraphWalker;
class PartiallyOpenedModulesList;
class PathName;
class Property;
class RefSignaturePool;
template<class Visitor>
class RootSetWalker;
class StackFrame;
class StackManager;
class StackTraceFormatter;
class StandardTypeCollection;
class StaticRef;
class StaticRefBlock;
class StaticStrings;
class StringBuffer;
class Thread;
class TryBlock;
class Type;
class VM;
struct CatchBlock;
struct CatchBlocks;
struct FinallyBlock;
struct StandardTypeInfo;

namespace debug
{
	class DebugSymbolsReader;
	class MethodSymbols;
	class ModuleDebugData;
	class OverloadSymbols;
	struct DebugSymbol;
	struct SourceFile;
	struct SourceLocation;
} // namespace ovum::debug

namespace instr
{
	class Instruction;
	class MethodBuffer;
	class MethodBuilder;
} // namespace ovum::instr

} // namespace ovum

// Define handle types in terms of internal VM types.

typedef ovum::Thread *const ThreadHandle;
typedef ovum::Type           *TypeHandle;
typedef ovum::Module         *ModuleHandle;
typedef ovum::Member         *MemberHandle;
typedef ovum::Method         *MethodHandle;
typedef ovum::MethodOverload *OverloadHandle;
typedef ovum::Field          *FieldHandle;
typedef ovum::Property       *PropertyHandle;

#define OVUM_HANDLES_DEFINED

// Public Ovum header.

#include "../inc/ovum.h"

// OS-specific code, which implements everything in ovum::os.

#if OVUM_WINDOWS
# include "os/windows.h"
#endif

// std::unique_ptr is used more or less everywhere, though aliased to ovum::Box.
#include <memory>
// Make placement new available everywhere too, especially std::nothrow.
#include <new>

namespace ovum
{
	// 'std::unique_ptr' is long and unsightly. For the benefit of everyone
	// reading the code, we take some inspiration from Rust and alias that
	// type to ovum::Box:
	template<class T, class Del = std::default_delete<T>>
	using Box = std::unique_ptr<T, Del>;
}
