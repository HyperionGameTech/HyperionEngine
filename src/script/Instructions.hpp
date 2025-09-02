#pragma once

#include <core/Types.hpp>

enum FunctionFlags : hyperion::uint8
{
    NONE = 0x00,
    VARIADIC = 0x01,
    GENERATOR = 0x02,
    CLOSURE = 0x04
};

// arguments should be placed in the format:
// dst, src

// parameters are marked in square brackets
// i8  = 8-bit integer (1 byte)
// i16 = 16-bit integer (2 bytes)
// i32 = 32-bit integer (4 bytes)
// i64 = 64-bit integer (8 bytes)
// u8  = 8-bit unsigned integer (1 byte)
// u16 = 16-bit unsigned integer (2 bytes)
// u32 = 32-bit unsigned integer (4 bytes)
// u64 = 64-bit unsigned integer (8 bytes)
// f32 = 32-bit float (4 bytes)
// f64 = 64-bit float (8 bytes)
// []  = array
// $   = stack offset (uint16_t)
// #   = static object index (uint16_t)
// %   = register (uint8_t)
// @   = address (uint32_t)

// NOTE: instructions that load data from stack index load from the main/global thread.
// instructions that load from stack offset load from their own thread.

enum Instructions : hyperion::uint8
{
    /* No operation */
    NOP = 0x00, // nop

    /* Unified load instruction with sub-command */
    LOAD_UNIFIED = 0x10, // [sub-cmd, reg, operand...]

    /* Unified move instruction with sub-command */
    MOV_UNIFIED = 0x11, // [sub-cmd, dst, src]

    /* Legacy load instructions (DEPRECATED - use LOAD_UNIFIED) */
    LOAD_I32,        // loadI32          [% reg, i32 val]
    LOAD_I64,        // loadI64          [% reg, i64 val]
    LOAD_U32,        // loadU32          [% reg, u32 val]
    LOAD_U64,        // loadU64          [% reg, u64 val]
    LOAD_F32,        // loadF32          [% reg, f32 val]
    LOAD_F64,        // loadF64          [% reg, f64 val]
    LOAD_OFFSET,     // loadOffset       [% reg, u16 offset]
    LOAD_INDEX,      // loadIndex        [% reg, u16 idx]
    LOAD_STATIC,     // loadStatic       [% reg, u16 idx]
    LOAD_STRING,     // loadStr          [% reg, u32 len, byte[len] str]
    LOAD_ADDR,       // loadAddr         [% reg, @ addr]
    LOAD_FUNC,       // loadFunc         [% reg, @ addr, u8 nargs, u8 flags]
    LOAD_ARRAYIDX,   // loadArrayidx     [% reg, % src, % idx]
    LOAD_OFFSET_REF, // loadOffsetRef   [% reg, u16 offset]
    LOAD_INDEX_REF,  // loadIndexRef    [% reg, u16 idx]
    LOAD_NULL,       // loadNull         [% reg]
    LOAD_TRUE,       // loadTrue         [% reg]
    LOAD_FALSE,      // loadFalse        [% reg]
    LOAD_CLASS,      // loadClass        [% reg, u64 nameHash]

    REF,   // ref               [% reg, % src]
    DEREF, // deref             [% reg, % src]

    /* Legacy move instructions (DEPRECATED - use MOV_UNIFIED) */
    MOV_OFFSET, // movOffset   [u16 dst, % src]
    /* Copy register value to stack index */
    MOV_INDEX, // movIndex    [u16 dst, % src]
    /* Copy register value to static index */
    MOV_STATIC, // movStatic   [u16 dst, % src]
    /* Copy register value to array index */
    MOV_ARRAYIDX, // movArrayidx [% dstArray, u32 dstIdx, %src]
    /* Copy register value to array index held in other register */
    MOV_ARRAYIDX_REG, // movArrayidxReg [% dstArray, % dstIdx, % src]
    /* Copy register value to another register */
    MOV, // mov      [% dst, % src]
    /* Check if the object in the register has a member with the hash,
        setting a boolean value in the dst register
    */

    /* Object member operations */
    /* Check if the object in %src has a member with the hash,
       setting a boolean value in %dst
    */
    CHECK_HAS_MEMBER, // checkHasMember [% dst, % src, u64 hash]
    /* Assign register value to object member (using hashcode) */
    SET_FIELD, // SET_FIELD [% dstObj, u64 hash, % src]
    /* Get member value and store it in a register (using hashcode) */
    GET_MEMBER, // getMember      [% reg, % src, u64 hash]

    /* Push a value from register to the stack */
    PUSH, // push [% src]
    /* Pop stack once */
    POP, // pop

    /* Push a value to the array in %dstArray */
    PUSH_ARRAY, // pushArray [% dst, % src]

    /* Add a value to the stack pointer */
    ADD_SP, // addSp [u16 val]
    /* Subtract a value from the stack pointer */
    SUB_SP, // subSp [u16 val]

    /* Jump to address stored in register */
    JMP, // jmp [% reg]
    JE,  // je [% reg]
    JNE, // jne [% reg]
    JG,  // jg [% reg]
    JGE, // jge [% reg]

    CALL, // call [% reg, u8 nargs]
    RET,  // ret

    BEGIN_TRY, // beginTry [% catchAddrReg]
    END_TRY,

    NEW,       // new [% dst, % srcTypeReg]
    NEW_ARRAY, // newArray [% dst, u32 size]

    BEGIN_CLASS, // beginClass [% dst, u16 nameLen, byte[nameLen] name, u16 size, { u16 len, byte[len] memberName }[size]]
    END_CLASS,   // endClass

    /* Compare two register values */
    CMP,  // cmp [% lhs, % rhs]
    CMPZ, // cmpz [% lhs]

    /* Mathematical operations */
    ADD, // add [% lhs, % rhs, % dst]
    SUB, // sub [% lhs, % rhs, % dst]
    MUL, // mul [% lhs, % rhs, % dst]
    DIV, // div [% lhs, % rhs, % dst]
    MOD, // mod [% lhs, % rhs, % dst]

    /* Bitwise operations */
    AND, // and [% lhs, % rhs, % dst]
    OR,  // or  [% lhs, % rhs, % dst]
    XOR, // xor [% lhs, % rhs, % dst]
    SHL, // shl [% lhs, % rhs, % dst]
    SHR, // shr [% lhs, % rhs, % dst]

    /* Unary operations */
    NEG, // neg [% src] - mathematical negation
    NOT, // not [% src] - bitwise complement

    THROW, // throw [% src] - throw an exception object stored in a register

    /* Binary to source trace map functionality */
    TRACEMAP, // tracemap [u32 length]

    /* Comment (for debugging) */
    REM, // rem [u32 len, byte[len] str]
    /* Export a symbol from register value by name */
    EXPORT, // export [% src, u32 len, byte[len] str]

    /* Casting with sub-command */
    CAST_UNIFIED = 0x90, // [sub-cmd, dst, src]

    /* Legacy cast instructions (DEPRECATED - use CAST_UNIFIED) */
    CAST_U8,  // castU8  [% dst, % src]
    CAST_U16, // castU16 [% dst, % src]
    CAST_U32, // castU32 [% dst, % src]
    CAST_U64, // castU64 [% dst, % src]

    CAST_I8,  // castI8  [% dst, % src]
    CAST_I16, // castI16 [% dst, % src]
    CAST_I32, // castI32 [% dst, % src]
    CAST_I64, // castI64 [% dst, % src]

    CAST_F32, // castF32 [% dst, % src]
    CAST_F64, // castF64 [% dst, % src]

    CAST_BOOL, // castBool [% dst, % src]

    CAST_DYNAMIC, // castDynamic [% dst, % src] - cast 'src' to dynamic type, the type is stored in the 'dst' register

    /* Signifies the end of the stream */
    EXIT = 0xFF
};

// Sub-command definitions for unified instructions

// Load sub-command format (8 bits):
// Bits 0-2: Data type (3 bits = 8 types)
// Bit 3: Reference flag (1 bit)
// Bits 4-6: Source type (3 bits = 8 source types)
// Bit 7: Extended flag (reserved)

enum LoadDataType : hyperion::uint8
{
    DTYPE_I32 = 0x00,
    DTYPE_I64 = 0x01,
    DTYPE_U32 = 0x02,
    DTYPE_U64 = 0x03,
    DTYPE_F32 = 0x04,
    DTYPE_F64 = 0x05,
    DTYPE_BOOL = 0x06,
    DTYPE_OBJECT = 0x07
};

enum LoadSourceType : hyperion::uint8
{
    LSRC_IMMEDIATE = 0x00, // Constant value follows
    LSRC_OFFSET = 0x01,    // Stack offset (uint16)
    LSRC_INDEX = 0x02,     // Stack index (uint16)
    LSRC_STATIC = 0x03,    // Static storage (uint16)
    LSRC_ARRAYIDX = 0x04,  // Array[index] (reg, reg)
    LSRC_MEMBER = 0x05,    // Object.member (reg, hash64)
    LSRC_REGISTER = 0x06,  // Another register (reg)
    LSRC_ADDRESS = 0x07    // Memory address (uint32)
};

// Move sub-command format (8 bits):
// Bits 0-1: Destination type (2 bits = 4 types)
// Bits 2-4: Source type (3 bits = 8 types)
// Bits 5-7: Reserved

enum MoveDstType : hyperion::uint8
{
    MDST_OFFSET = 0x00,   // Stack offset
    MDST_INDEX = 0x01,    // Stack index
    MDST_STATIC = 0x02,   // Static storage
    MDST_REGISTER = 0x03  // Register
};

enum MoveSrcType : hyperion::uint8
{
    MSRC_REGISTER = 0x00,     // Register
    MSRC_ARRAYIDX = 0x01,     // Array index (immediate) - load FROM array
    MSRC_ARRAYIDX_REG = 0x02, // Array index (register) - load FROM array
    MSRC_MEMBER = 0x03,       // Object member
    MSRC_TO_ARRAYIDX = 0x04   // Store TO array index (register source)
    // Reserved slots for future use
};

// Cast sub-command format (8 bits):
// Bits 0-3: Target type (4 bits = 16 types)
// Bits 4-7: Reserved

enum CastType : hyperion::uint8
{
    CAST_TYPE_U8 = 0x00,
    CAST_TYPE_U16 = 0x01,
    CAST_TYPE_U32 = 0x02,
    CAST_TYPE_U64 = 0x03,
    CAST_TYPE_I8 = 0x04,
    CAST_TYPE_I16 = 0x05,
    CAST_TYPE_I32 = 0x06,
    CAST_TYPE_I64 = 0x07,
    CAST_TYPE_F32 = 0x08,
    CAST_TYPE_F64 = 0x09,
    CAST_TYPE_BOOL = 0x0A,
    CAST_TYPE_DYNAMIC = 0x0B
    // 4 reserved slots
};

// Helper macros for creating sub-commands
#define MAKE_LOAD_SUBCMD(dataType, isRef, srcType) \
    ((hyperion::uint8)((dataType) | ((isRef) << 3) | ((srcType) << 4)))

#define MAKE_MOV_SUBCMD(dstType, srcType) \
    ((hyperion::uint8)((dstType) | ((srcType) << 2)))

#define MAKE_CAST_SUBCMD(castType) \
    ((hyperion::uint8)(castType))

// Extract fields from sub-commands
#define GET_LOAD_DTYPE(subcmd) ((subcmd) & 0x07)
#define GET_LOAD_ISREF(subcmd) (((subcmd) >> 3) & 0x01)
#define GET_LOAD_SRCTYPE(subcmd) (((subcmd) >> 4) & 0x07)

#define GET_MOV_DSTTYPE(subcmd) ((subcmd) & 0x03)
#define GET_MOV_SRCTYPE(subcmd) (((subcmd) >> 2) & 0x07)

#define GET_CAST_TYPE(subcmd) ((subcmd) & 0x0F)
