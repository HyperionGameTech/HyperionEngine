#ifndef INSTRUCTIONS_HPP
#define INSTRUCTIONS_HPP

#include <stdint.h>

using bc_address_t = uint32_t;
using bc_reg_t = uint8_t;

enum class DataStoreMode {
    DataStoreString = 0x00,
    DataStoreAddress = 0x01,
    DataStoreFunction = 0x02,
    DataStoreType = 0x03
};

enum class MovRegMode {
    RegToStackOffset = 0x00,
    StackOffsetToReg = 0x01,
    RegToStackIndex = 0x02,
    StackIndexToReg = 0x03
};

enum class MovIndexMode {
    RegToIndex = 0x00,
    IndexToReg = 0x01,
};

enum class JumpMode {
    JumpEqual = 0x00,
    JumpNotEqual = 0x01,
    JumpGreater = 0x02,
    JumpGreaterEqual = 0x03
};

enum class Instructions_2 {
    NOP = 0x00,

    STORE,

    CONST_I32,
    CONST_I64,
    CONST_F32,
    CONST_F64,
    CONST_BOOLEAN,
    CONST_NULL,

    MOV_REG,
    MOV_INDEX,

    JMP,
    JMP_IF,

    PUSH,
    POP,

    CMP,
    CMPZ,

    CALL,
    RET,

    PRINT,

    ADD,
    SUB,
    MUL,
    DIV,
    MOD,

    NEG,
    NOT,

    HALT,
};



enum FunctionFlags : uint8_t {
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

enum Instructions : char {
    /* No operation */
    NOP = 0x00, // nop

    /* Store values in static memory */
    STORE_STATIC_STRING,   // str      [u32 len, byte[len] str]
    STORE_STATIC_ADDRESS,  // addr     [@ addr]
    STORE_STATIC_FUNCTION, // function [@ addr, u8 nargs, u8 flags]
    STORE_STATIC_TYPE,     // type     [u16 name_len, byte[name_len] name, u16 size, { u16 len, byte[len] member_name }[size]]

    /* Load a value into a register */
    LOAD_I32,      // load_i32      [% reg, i32 val]
    LOAD_I64,      // load_i64      [% reg, i64 val]
    LOAD_F32,      // load_f32      [% reg, f32 val]
    LOAD_F64,      // load_f64      [% reg, f64 val]
    LOAD_OFFSET,   // load_offset   [% reg, u16 offset]
    LOAD_INDEX,    // load_index    [% reg, u16 idx]
    LOAD_STATIC,   // load_static   [% reg, u16 idx]
    LOAD_STRING,   // load_str      [% reg, u32 len, byte[len] str]
    LOAD_ADDR,     // load_addr     [% reg, @ addr]
    LOAD_FUNC,     // load_func     [% reg, @ addr, u8 nargs, u8 flags]
    LOAD_TYPE,     // load_type     [% reg, u16 name_len, byte[name_len] name, u16 size, { u16 len, byte[len] member_name }[size]]
    LOAD_MEM,      // load_mem      [% reg, % src, u8 idx]
    LOAD_MEM_HASH, // load_mem_hash [% reg, % src, u32 hash]
    LOAD_ARRAYIDX, // load_arrayidx [% reg, % src, % idx]
    LOAD_REF,      // load_ref      [% reg, % src]
    LOAD_DEREF,    // load_deref    [% reg, % src]
    LOAD_NULL,     // load_null     [% reg]
    LOAD_TRUE,     // load_true     [% reg]
    LOAD_FALSE,    // load_false    [% reg]

    /* Copy register value to stack offset */
    MOV_OFFSET,     // mov_offset   [u16 dst, % src]
    /* Copy register value to stack index */
    MOV_INDEX,      // mov_index    [u16 dst, % src]
    /* Copy register value to object member */
    MOV_MEM,        // mov_mem      [% dst_obj, u8 dst_idx, % src]
    /* Copy register value to object member (using hashcode) */
    MOV_MEM_HASH,   // mov_mem_hash [% dst_obj, u32 hash, % src]
    /* Copy register value to array index */
    MOV_ARRAYIDX,   // mov_arrayidx [% dst_array, u32 dst_idx, %src]
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
    /* Pop stack n amount of times */
    POP_N, // pop_n [u8 n]

    /* Push a value to the array in %dst_array */
    PUSH_ARRAY, // push_array [% dst, % src]

    ECHO,         // echo [% reg]
    ECHO_NEWLINE, // echo_newline

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
    NEW_PROTO, // new_proto [% dst, % src]
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

    /* Signifies the end of the stream */
    EXIT,
};

#endif
