#pragma once

#ifndef VM__VM_OPCODES_H
#define VM__VM_OPCODES_H

#include "ov_vm.internal.h"
#include <vector>
#include <cassert>

namespace ovum
{

enum Opcode : uint8_t
{
	OPC_NOP       = 0x00,
	OPC_DUP       = 0x01,
	OPC_POP       = 0x02,
	// Arguments
	OPC_LDARG_0   = 0x03,
	OPC_LDARG_1   = 0x04,
	OPC_LDARG_2   = 0x05,
	OPC_LDARG_3   = 0x06,
	OPC_LDARG_S   = 0x07,
	OPC_LDARG     = 0x08,
	OPC_STARG_S   = 0x09,
	OPC_STARG     = 0x0a,
	// Locals
	OPC_LDLOC_0   = 0x0b,
	OPC_LDLOC_1   = 0x0c,
	OPC_LDLOC_2   = 0x0d,
	OPC_LDLOC_3   = 0x0e,
	OPC_STLOC_0   = 0x0f,
	OPC_STLOC_1   = 0x10,
	OPC_STLOC_2   = 0x11,
	OPC_STLOC_3   = 0x12,
	OPC_LDLOC_S   = 0x13,
	OPC_LDLOC     = 0x14,
	OPC_STLOC_S   = 0x15,
	OPC_STLOC     = 0x16,
	// Values and object initialisation
	OPC_LDNULL    = 0x17,
	OPC_LDFALSE   = 0x18,
	OPC_LDTRUE    = 0x19,
	OPC_LDC_I_M1  = 0x1a,
	OPC_LDC_I_0   = 0x1b,
	OPC_LDC_I_1   = 0x1c,
	OPC_LDC_I_2   = 0x1d,
	OPC_LDC_I_3   = 0x1e,
	OPC_LDC_I_4   = 0x1f,
	OPC_LDC_I_5   = 0x20,
	OPC_LDC_I_6   = 0x21,
	OPC_LDC_I_7   = 0x22,
	OPC_LDC_I_8   = 0x23,
	OPC_LDC_I_S   = 0x24,
	OPC_LDC_I_M   = 0x25,
	OPC_LDC_I     = 0x26,
	OPC_LDC_U     = 0x27,
	OPC_LDC_R     = 0x28,
	OPC_LDSTR     = 0x29,
	OPC_LDARGC    = 0x2a,
	OPC_LDENUM_S  = 0x2b,
	OPC_LDENUM    = 0x2c,
	OPC_NEWOBJ_S  = 0x2d,
	OPC_NEWOBJ    = 0x2e,
	// Invocation
	OPC_CALL_0    = 0x2f,
	OPC_CALL_1    = 0x30,
	OPC_CALL_2    = 0x31,
	OPC_CALL_3    = 0x32,
	OPC_CALL_S    = 0x33,
	OPC_CALL      = 0x34,
	OPC_SCALL_S   = 0x35,
	OPC_SCALL     = 0x36,
	OPC_APPLY     = 0x37,
	OPC_SAPPLY    = 0x38,
	// Control flow
	OPC_RETNULL   = 0x39,
	OPC_RET       = 0x3a,
	OPC_BR_S      = 0x3b,
	OPC_BRNULL_S  = 0x3c,
	OPC_BRINST_S  = 0x3d,
	OPC_BRFALSE_S = 0x3e,
	OPC_BRTRUE_S  = 0x3f,
	OPC_BRREF_S   = 0x40,
	OPC_BRNREF_S  = 0x41,
	OPC_BRTYPE_S  = 0x42,
	OPC_BR        = 0x43,
	OPC_BRNULL    = 0x44,
	OPC_BRINST    = 0x45,
	OPC_BRFALSE   = 0x46,
	OPC_BRTRUE    = 0x47,
	OPC_BRREF     = 0x48,
	OPC_BRNREF    = 0x49,
	OPC_BRTYPE    = 0x4a,
	OPC_SWITCH_S  = 0x4b,
	OPC_SWITCH    = 0x4c,
	// Operators
	OPC_ADD        = 0x4d,
	OPC_SUB        = 0x4e,
	OPC_OR         = 0x4f,
	OPC_XOR        = 0x50,
	OPC_MUL        = 0x51,
	OPC_DIV        = 0x52,
	OPC_MOD        = 0x53,
	OPC_AND        = 0x54,
	OPC_POW        = 0x55,
	OPC_SHL        = 0x56,
	OPC_SHR        = 0x57,
	OPC_HASHOP     = 0x58,
	OPC_DOLLAR     = 0x59,
	OPC_PLUS       = 0x5a,
	OPC_NEG        = 0x5b,
	OPC_NOT        = 0x5c,
	OPC_EQ         = 0x5d,
	OPC_CMP        = 0x5e,
	OPC_LT         = 0x5f,
	OPC_GT         = 0x60,
	OPC_LTE        = 0x61,
	OPC_GTE        = 0x62,
	OPC_CONCAT     = 0x63,
	// Misc. data
	OPC_LIST_0     = 0x64,
	OPC_LIST_S     = 0x65,
	OPC_LIST       = 0x66,
	OPC_HASH_0     = 0x67,
	OPC_HASH_S     = 0x68,
	OPC_HASH       = 0x69,
	OPC_LDITER     = 0x6a,
	OPC_LDTYPE     = 0x6b,
	// Fields
	OPC_LDFLD      = 0x6c,
	OPC_STFLD      = 0x6d,
	OPC_LDSFLD     = 0x6e,
	OPC_STSFLD     = 0x6f,
	// Named member access
	OPC_LDMEM      = 0x70,
	OPC_STMEM      = 0x71,
	// Indexers
	OPC_LDIDX_1    = 0x72,
	OPC_LDIDX_S    = 0x73,
	OPC_LDIDX      = 0x74,
	OPC_STIDX_1    = 0x75,
	OPC_STIDX_S    = 0x76,
	OPC_STIDX      = 0x77,
	// Global/static functions
	OPC_LDSFN      = 0x78,
	// Type tokens
	OPC_LDTYPETKN  = 0x79,
	// Exception handling
	OPC_THROW      = 0x7a,
	OPC_RETHROW    = 0x7b,
	OPC_LEAVE_S    = 0x7c,
	OPC_LEAVE      = 0x7d,
	OPC_ENDFINALLY = 0x7e,
	// Call member
	OPC_CALLMEM_S  = 0x7f,
	OPC_CALLMEM    = 0x80,
	// References
	OPC_LDMEMREF   = 0x81,
	OPC_LDARGREF_S = 0x82,
	OPC_LDARGREF   = 0x83,
	OPC_LDLOCREF_S = 0x84,
	OPC_LDLOCREF   = 0x85,
	OPC_LDFLDREF   = 0x86,
	OPC_LDSFLDREF  = 0x87,
};

// Intermediate opcodes are generated by the method initializer,
// and are used to keep the jump table in Thread::Evaluate small.
// Since we need to mangle the bytecode anyway, it makes no real
// sense to keep a less optimised (for execution time) format.
enum IntermediateOpcode : uint8_t
{ 
	OPI_NOP         = 0x00,
	OPI_POP         = 0x01,

	// These are put here mostly to use up 0x02 and 0x03.
	OPI_RET         = 0x02,
	OPI_RETNULL     = 0x03,

	// mvloc encodes the stack change in its lowest two bits:
	// 0000 001ar
	//         a  = if set, one value was added
	//          r = if set, one value was removed
	OPI_MVLOC_LL    = 0x04, // Local -> local, no stack change
	OPI_MVLOC_SL    = 0x05, // Stack -> local, one value was removed
	OPI_MVLOC_LS    = 0x06, // Local -> stack, one value was added
	OPI_MVLOC_SS    = 0x07, // Stack -> stack, not used (what would this even mean?)
	// OPI_MVLOC_LS is also used for dup, with the "source" local pointing to
	// a stack slot, since the effective stack change is +1.

	// In the constant loading instructions, the lowest bit
	// indicates whether the instruction loads anything onto
	// the stack: if set, the stack height is incremented.
	OPI_LDNULL_L    = 0x08, // -> local
	OPI_LDNULL_S    = 0x09, // -> stack
	// Note: 0x0a = 0000 1010
	//       0x0b = 0000 1011
	//       0x0c = 0000 1100
	//       0x0d = 0000 1101
	// This means we can use the third lowest bit as a true/false flag,
	// hence why ldfalse comes first.
	// Yes, I could reverse the order and use the second lowest bit,
	// but it makes more sense for false to come first.
	OPI_LDFALSE_L   = 0x0a,
	OPI_LDFALSE_S   = 0x0b,
	OPI_LDTRUE_L    = 0x0c,
	OPI_LDTRUE_S    = 0x0d,
	// Int
	OPI_LDC_I_L     = 0x0e,
	OPI_LDC_I_S     = 0x0f,
	// UInt
	OPI_LDC_U_L     = 0x10,
	OPI_LDC_U_S     = 0x11,
	// Real
	OPI_LDC_R_L     = 0x12,
	OPI_LDC_R_S     = 0x13,
	// String
	OPI_LDSTR_L     = 0x14,
	OPI_LDSTR_S     = 0x15,
	// Argument count
	OPI_LDARGC_L    = 0x16,
	OPI_LDARGC_S    = 0x17,
	// Enum value
	OPI_LDENUM_L    = 0x18,
	OPI_LDENUM_S    = 0x19,
	// New object
	OPI_NEWOBJ_L    = 0x1a, // Store new object in local
	OPI_NEWOBJ_S    = 0x1b, // Store new object on stack
	// List
	OPI_LIST_L      = 0x1c,
	OPI_LIST_S      = 0x1d,
	// Hash
	OPI_HASH_L      = 0x1e,
	OPI_HASH_S      = 0x1f,
	// Field
	OPI_LDFLD_L     = 0x20,
	OPI_LDFLD_S     = 0x21,
	// Static field
	OPI_LDSFLD_L    = 0x22,
	OPI_LDSFLD_S    = 0x23,
	// Member
	OPI_LDMEM_L     = 0x24,
	OPI_LDMEM_S     = 0x25,
	// Iterator
	OPI_LDITER_L    = 0x26,
	OPI_LDITER_S    = 0x27,
	// Type token (from value)
	OPI_LDTYPE_L    = 0x28,
	OPI_LDTYPE_S    = 0x29,
	// Indexer
	OPI_LDIDX_L     = 0x2a,
	OPI_LDIDX_S     = 0x2b,
	// Static function
	OPI_LDSFN_L     = 0x2c,
	OPI_LDSFN_S     = 0x2d,
	// Type token (from Type*)
	OPI_LDTYPETKN_L = 0x2e,
	OPI_LDTYPETKN_S = 0x2f,

	// Invocation
	OPI_CALL_L      = 0x30, // Store return value in local
	OPI_CALL_S      = 0x31, // Store return value on stack

	OPI_SCALL_L     = 0x32, // Store return value in local
	OPI_SCALL_S     = 0x33, // Store return value on stack

	OPI_APPLY_L     = 0x34, // Store return value in local
	OPI_APPLY_S     = 0x35, // Store return value on stack

	OPI_SAPPLY_L    = 0x36, // Store return value in local
	OPI_SAPPLY_S    = 0x37, // Store return value on stack 

	// Unconditional branches
	OPI_BR          = 0x38,
	OPI_LEAVE       = 0x39,

	// In the following branch instructions, except brref and brnref
	// which always read from the stack, the lowest bit encodes the
	// source of the branch value: if set, the instruction takes one
	// value from the stack, otherwise it's read from a local.
	OPI_BRNULL_L    = 0x3a,
	OPI_BRNULL_S    = 0x3b,

	OPI_BRINST_L    = 0x3c,
	OPI_BRINST_S    = 0x3d,

	OPI_BRFALSE_L   = 0x3e,
	OPI_BRFALSE_S   = 0x3f,

	OPI_BRTRUE_L    = 0x40,
	OPI_BRTRUE_S    = 0x41,

	OPI_BRTYPE_L    = 0x42,
	OPI_BRTYPE_S    = 0x43,

	OPI_SWITCH_L    = 0x44,
	OPI_SWITCH_S    = 0x45,

	OPI_BRREF       = 0x46,
	OPI_BRNREF      = 0x47,

	// Operators! Hurrah! (Note that the comparison operators � ==, <=>, <, >, <=, >= � and concat are still handled specially)
	OPI_OPERATOR_L  = 0x48, // Store result in local
	OPI_OPERATOR_S  = 0x49, // Store result on stack

	OPI_EQ_L        = 0x4a, // Store result in local
	OPI_EQ_S        = 0x4b, // Store result on stack
	OPI_CMP_L       = 0x4c, // <=>
	OPI_CMP_S       = 0x4d,
	OPI_LT_L        = 0x4e, // <
	OPI_LT_S        = 0x4f,
	OPI_GT_L        = 0x50, // >
	OPI_GT_S        = 0x51,
	OPI_LTE_L       = 0x52, // <=
	OPI_LTE_S       = 0x53,
	OPI_GTE_L       = 0x54, // >=
	OPI_GTE_S       = 0x55,

	OPI_CONCAT_L    = 0x56, // Store result in local
	OPI_CONCAT_S    = 0x57, // Stack on result store

	// Call member
	OPI_CALLMEM_L   = 0x58, // Store result in local
	OPI_CALLMEM_S   = 0x59, // Store result on stack

	// Store instructions (for things other than locals)
	OPI_STSFLD_L    = 0x5a, // Get value from local
	OPI_STSFLD_S    = 0x5b, // Get value from stack

	OPI_STFLD       = 0x5c, // Always get values from stack
	OPI_STMEM       = 0x5d, // Same here
	OPI_STIDX       = 0x5e, // And here

	// Some general simple instructions
	OPI_THROW       = 0x5f,
	OPI_RETHROW     = 0x60,
	OPI_ENDFINALLY  = 0x61,

	// Optimised field loading/storing
	OPI_LDFLDFAST_L = 0x62,
	OPI_LDFLDFAST_S = 0x63,
	OPI_STFLDFAST   = 0x64,

	// Conditional comparison branches
	OPI_BREQ        = 0x65, // ==
	OPI_BRNEQ       = 0x66, // !=
	OPI_BRLT        = 0x67, // <
	OPI_BRGT        = 0x68, // >
	OPI_BRLTE       = 0x69, // <=
	OPI_BRGTE       = 0x6a, // >=
	OPI_BRNLT       = OPI_BRGTE,
	OPI_BRNGT       = OPI_BRLTE,
	OPI_BRNLTE      = OPI_BRGT,
	OPI_BRNGTE      = OPI_BRLT,

	// Reference loading (output always on stack)
	OPI_LDLOCREF    = 0x6b,
	OPI_LDMEMREF_L  = 0x6c, // Instance in local
	OPI_LDMEMREF_S  = 0x6d, // Instance on stack
	OPI_LDFLDREF_L  = 0x6e, // Instance in local
	OPI_LDFLDREF_S  = 0x6f, // Instance on stack
	OPI_LDSFLDREF   = 0x70,

	// Moving to/from references (with dereferencing)
	OPI_MVLOC_RL    = 0x72, // Reference -> local
	OPI_MVLOC_RS    = 0x73, // Reference -> stack
	OPI_MVLOC_LR    = 0x74, // Local -> reference
	OPI_MVLOC_SR    = 0x75, // Stack -> reference

	// Call with references
	OPI_CALLR_L     = 0x76,
	OPI_CALLR_S     = 0x77,
	OPI_CALLMEMR_L  = 0x78,
	OPI_CALLMEMR_S  = 0x79,
};

// Represents a local offset, that is, an offset that is relative
// to the base of the stack frame. This is negative for arguments.
// Use the overloaded + operator together with a StackFrame to get
// the local that it actually refers to.
class LocalOffset
{
private:
	int32_t offset;

public:
	inline LocalOffset(const int32_t offset) : offset(offset) { }

	inline int32_t GetOffset() const { return offset; }

	inline Value *const operator+(const StackFrame *const frame) const
	{
		// The local offset is never supposed to point into
		// the stack frame itself.
		assert(offset < 0 || offset >= STACK_FRAME_SIZE);
		return (Value*)((char*)frame + offset);
	}
};

} // namespace ovum

#endif // VM__VM_OPCODES_H