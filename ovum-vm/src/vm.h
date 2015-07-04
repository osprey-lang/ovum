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

// Forward declarations of fundamental VM classes and structs.

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
	class DebugSymbols;
	class ModuleDebugData;
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

#include "../inc/ov_vm.h"

// OS-specific code, which also implements everything in ovum::os.

#if OVUM_WINDOWS
# if !defined(UNICODE) || !defined(_UNICODE)
#  error Ovum on Windows must be compiled with Unicode support; make sure UNICODE and _UNICODE are defined.
# endif
# include "os/windows.h"
#endif

// Some fundamental typedefs. Should probably eventually be removed from this file.

namespace ovum
{

typedef uint32_t TokenId;

} // namespace ovum

#endif // VM__VM_INTERNAL
