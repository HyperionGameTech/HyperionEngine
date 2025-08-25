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

    /* Store values in static memory */
    STORE_STATIC_STRING,   // str      [u32 len, byte[len] str]
    STORE_STATIC_ADDRESS,  // addr     [@ addr]
    STORE_STATIC_FUNCTION, // function [@ addr, u8 nargs, u8 flags]
    STORE_STATIC_TYPE,     // type     [u16 nameLen, byte[nameLen] name, u16 size, { u16 len, byte[len] memberName }[size]]

    /* Load a value into a register */
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
    LOAD_TYPE,       // loadType         [% reg, u16 nameLen, byte[nameLen] name, u16 size, { u16 len, byte[len] memberName }[size]]
    LOAD_MEM,        // loadMem          [% reg, % src, u8 idx]
    LOAD_MEM_HASH,   // loadMemHash     [% reg, % src, u64 hash]
    LOAD_ARRAYIDX,   // loadArrayidx     [% reg, % src, % idx]
    LOAD_OFFSET_REF, // loadOffsetRef   [% reg, u16 offset]
    LOAD_INDEX_REF,  // loadIndexRef    [% reg, u16 idx]
    LOAD_NULL,       // loadNull         [% reg]
    LOAD_TRUE,       // loadTrue         [% reg]
    LOAD_FALSE,      // loadFalse        [% reg]

    REF,   // ref               [% reg, % src]
    DEREF, // deref             [% reg, % src]

    /* Copy register value to stack offset */
    MOV_OFFSET, // movOffset   [u16 dst, % src]
    /* Copy register value to stack index */
    MOV_INDEX, // movIndex    [u16 dst, % src]
    /* Copy register value to static index */
    MOV_STATIC, // movStatic   [u16 dst, % src]
    /* Copy register value to object member */
    MOV_MEM, // movMem      [% dstObj, u8 dstIdx, % src]
    /* Copy register value to object member (using hashcode) */
    MOV_MEM_HASH, // movMemHash [% dstObj, u64 hash, % src]
    /* Copy register value to array index */
    MOV_ARRAYIDX, // movArrayidx [% dstArray, u32 dstIdx, %src]
    /* Copy register value to array index held in other register */
    MOV_ARRAYIDX_REG, // movArrayidxReg [% dstArray, % dstIdx, % src]
    /* Copy register value to another register */
    MOV_REG, // movReg      [% dst, % src]
    /* Check if the object in the register has a member with the hash,
        setting a boolean value in the dst register
    */
    HAS_MEM_HASH, // hasMemHash [% dst, % src, u64 hash]

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

    /* Casts */
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
