#ifndef INSTRUCTIONS_HPP
#define INSTRUCTIONS_HPP

#include <Types.hpp>

enum FunctionFlags : hyperion::UInt8
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

enum Instructions : hyperion::UInt8
{
    /* No operation */
    NOP = 0x00, // nop

    /* Store values in static memory */
    STORE_STATIC_STRING,   // str      [u32 len, byte[len] str]
    STORE_STATIC_ADDRESS,  // addr     [@ addr]
    STORE_STATIC_FUNCTION, // function [@ addr, u8 nargs, u8 flags]
    STORE_STATIC_TYPE,     // type     [u16 name_len, byte[name_len] name, u16 size, { u16 len, byte[len] member_name }[size]]

    /* Load a value into a register */
    LOAD_I32,          // load_i32          [% reg, i32 val]
    LOAD_I64,          // load_i64          [% reg, i64 val]
    LOAD_U32,          // load_u32          [% reg, u32 val]
    LOAD_U64,          // load_u64          [% reg, u64 val]
    LOAD_F32,          // load_f32          [% reg, f32 val]
    LOAD_F64,          // load_f64          [% reg, f64 val]
    LOAD_OFFSET,       // load_offset       [% reg, u16 offset]
    LOAD_INDEX,        // load_index        [% reg, u16 idx]
    LOAD_STATIC,       // load_static       [% reg, u16 idx]
    LOAD_STRING,       // load_str          [% reg, u32 len, byte[len] str]
    LOAD_ADDR,         // load_addr         [% reg, @ addr]
    LOAD_FUNC,         // load_func         [% reg, @ addr, u8 nargs, u8 flags]
    LOAD_TYPE,         // load_type         [% reg, u16 name_len, byte[name_len] name, u16 size, { u16 len, byte[len] member_name }[size]]
    LOAD_MEM,          // load_mem          [% reg, % src, u8 idx]
    LOAD_MEM_HASH,     // load_mem_hash     [% reg, % src, u32 hash]
    LOAD_ARRAYIDX,     // load_arrayidx     [% reg, % src, % idx]
    LOAD_OFFSET_REF,   // load_offset_ref   [% reg, u16 offset]
    LOAD_INDEX_REF,    // load_index_ref    [% reg, u16 idx]
    LOAD_NULL,         // load_null         [% reg]
    LOAD_TRUE,         // load_true         [% reg]
    LOAD_FALSE,        // load_false        [% reg]

    REF,               // ref               [% reg, % src]
    DEREF,             // deref             [% reg, % src]

    /* Copy register value to stack offset */
    MOV_OFFSET,     // mov_offset   [u16 dst, % src]
    /* Copy register value to stack index */
    MOV_INDEX,      // mov_index    [u16 dst, % src]
    /* Copy register value to static index */
    MOV_STATIC,     // mov_static   [u16 dst, % src]
    /* Copy register value to object member */
    MOV_MEM,        // mov_mem      [% dst_obj, u8 dst_idx, % src]
    /* Copy register value to object member (using hashcode) */
    MOV_MEM_HASH,   // mov_mem_hash [% dst_obj, u32 hash, % src]
    /* Copy register value to array index */
    MOV_ARRAYIDX,   // mov_arrayidx [% dst_array, u32 dst_idx, %src]
    /* Copy register value to array index held in other register */
    MOV_ARRAYIDX_REG, // mov_arrayidx_reg [% dst_array, % dst_idx, % src]
    /* Copy register value to another register */
    MOV_REG,        // mov_reg      [% dst, % src]
    /* Check if the object in the register has a member with the hash,
        setting a boolean value in the dst register
    */
    HAS_MEM_HASH, // has_mem_hash [% dst, % src, u32 hash]

    /* Push a value from register to the stack */
    PUSH,  // push [% src]
    /* Pop stack once */
    POP,   // pop

    /* Push a value to the array in %dst_array */
    PUSH_ARRAY, // push_array [% dst, % src]

    /* Add a value to the stack pointer */
    ADD_SP, // add_sp [u16 val]
    /* Subtract a value from the stack pointer */
    SUB_SP, // sub_sp [u16 val]

    /* Jump to address stored in register */
    JMP, // jmp [% reg]
    JE,  // je [% reg]
    JNE, // jne [% reg]
    JG,  // jg [% reg]
    JGE, // jge [% reg]

    CALL, // call [% reg, u8 nargs]
    RET,  // ret

    BEGIN_TRY, // begin_try [% catch_addr_reg]
    END_TRY,

    NEW,       // new [% dst, % src_type_reg]
    NEW_ARRAY, // new_array [% dst, u32 size]

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
    CAST_U8,  // cast_u8  [% dst, % src]
    CAST_U16, // cast_u16 [% dst, % src]
    CAST_U32, // cast_u32 [% dst, % src]
    CAST_U64, // cast_u64 [% dst, % src]

    CAST_I8,  // cast_i8  [% dst, % src]
    CAST_I16, // cast_i16 [% dst, % src]
    CAST_I32, // cast_i32 [% dst, % src]
    CAST_I64, // cast_i64 [% dst, % src]

    CAST_F32, // cast_f32 [% dst, % src]
    CAST_F64, // cast_f64 [% dst, % src]

    CAST_BOOL, // cast_bool [% dst, % src]

    CAST_DYNAMIC, // cast_dynamic [% dst, % src] - cast 'src' to dynamic type, the type is stored in the 'dst' register

    /* Signifies the end of the stream */
    EXIT = 0xFF
};

#endif
