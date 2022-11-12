#ifndef INSTRUCTION_HANDLER_HPP
#define INSTRUCTION_HANDLER_HPP

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMMemoryBuffer.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMTypeInfo.hpp>

#include <script/Instructions.hpp>
#include <Types.hpp>
#include <script/Hasher.hpp>

#include <util/Defines.hpp>
#include <system/Debug.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>

#define HYP_NUMERIC_OPERATION(a, b, oper) \
    do { \
        switch (result.m_type) { \
            case Value::I32: \
                result.m_value.i32 = static_cast<Int32>(a.i) oper static_cast<Int32>(b.i); \
                break; \
            case Value::I64: \
                result.m_value.i64 = a.i oper b.i; \
                break; \
            case Value::U32: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 = static_cast<UInt32>(a.i); \
                } else { \
                    result.m_value.u32 = static_cast<UInt32>(a.u); \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 oper##= static_cast<UInt32>(b.i); \
                } else { \
                    result.m_value.u32 oper##= static_cast<UInt32>(b.u); \
                } \
                break; \
            case Value::U64: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 = static_cast<UInt64>(a.i); \
                } else { \
                    result.m_value.u64 = a.u; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 oper##= static_cast<UInt64>(b.i); \
                } else { \
                    result.m_value.u64 oper##= b.u; \
                } \
                break; \
            case Value::F32: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.f = static_cast<Float32>(a.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.f = static_cast<Float32>(a.u); \
                } else { \
                    result.m_value.f = static_cast<Float32>(a.f); \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.f oper##= static_cast<Float32>(b.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.f oper##= static_cast<Float32>(b.u); \
                } else { \
                    result.m_value.f oper##= static_cast<Float32>(b.f); \
                } \
                break; \
            case Value::F64: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.d = static_cast<Float64>(a.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.d = static_cast<Float64>(a.u); \
                } else { \
                    result.m_value.d = a.f; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.d oper##= static_cast<Float64>(b.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.d oper##= static_cast<Float64>(b.u); \
                } else { \
                    result.m_value.d oper##= b.f; \
                } \
                break; \
            default: \
                AssertThrowMsg(false, "Invalid type, should not reach this state."); \
                break; \
        } \
    } while (0)

#define HYP_NUMERIC_OPERATION_BITWISE(a, b, oper) \
    do { \
        switch (result.m_type) { \
            case Value::I32: \
                result.m_value.i32 = static_cast<Int32>(a.i) oper static_cast<Int32>(b.i); \
                break; \
            case Value::I64: \
                result.m_value.i64 = a.i oper b.i; \
                break; \
            case Value::U32: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 = static_cast<UInt32>(a.i); \
                } else { \
                    result.m_value.u32 = static_cast<UInt32>(a.u); \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 oper##= static_cast<UInt32>(b.i); \
                } else { \
                    result.m_value.u32 oper##= static_cast<UInt32>(b.u); \
                } \
                break; \
            case Value::U64: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 = static_cast<UInt64>(a.i); \
                } else { \
                    result.m_value.u64 = a.u; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 oper##= static_cast<UInt64>(b.i); \
                } else { \
                    result.m_value.u64 oper##= b.u; \
                } \
                break; \
            default: \
                state->ThrowException(thread, Exception::InvalidBitwiseArgument()); \
                break; \
        } \
    } while (0)

namespace hyperion {
namespace vm {

class InstructionHandler
{
public:
    VMState *state;
    ExecutionThread *thread;
    BytecodeStream *bs;

    InstructionHandler(VMState *state,
      ExecutionThread *thread,
      BytecodeStream *bs)
      : state(state),
        thread(thread),
        bs(bs)
    {
    }

    HYP_FORCE_INLINE void StoreStaticString(uint32_t len, const char *str)
    {
        // the value will be freed on
        // the destructor call of state->m_static_memory
        HeapValue *hv = new HeapValue();
        hv->Assign(VMString(str));

        Value sv;
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;

        state->m_static_memory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void StoreStaticAddress(BCAddress addr)
    {
        Value sv;
        sv.m_type = Value::ADDRESS;
        sv.m_value.addr = addr;

        state->m_static_memory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void StoreStaticFunction(BCAddress addr,
        uint8_t nargs,
        uint8_t flags)
    {
        Value sv;
        sv.m_type = Value::FUNCTION;
        sv.m_value.func.m_addr = addr;
        sv.m_value.func.m_nargs = nargs;
        sv.m_value.func.m_flags = flags;

        state->m_static_memory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void StoreStaticType(const char *type_name,
        uint16_t size,
        char **names)
    {
        // the value will be freed on
        // the destructor call of state->m_static_memory
        HeapValue *hv = new HeapValue();
        hv->Assign(VMTypeInfo(type_name, size, names));

        Value sv;
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;
        state->m_static_memory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void LoadI32(BCRegister reg, Int32 i32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::I32;
        value.m_value.i32 = i32;
    }

    HYP_FORCE_INLINE void LoadI64(BCRegister reg, Int64 i64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::I64;
        value.m_value.i64 = i64;
    }

    HYP_FORCE_INLINE void LoadU32(BCRegister reg, UInt32 u32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::U32;
        value.m_value.u32 = u32;
    }

    HYP_FORCE_INLINE void LoadU64(BCRegister reg, UInt64 u64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::U64;
        value.m_value.u64 = u64;
    }

    HYP_FORCE_INLINE void LoadF32(BCRegister reg, Float32 f32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::F32;
        value.m_value.f = f32;
    }

    HYP_FORCE_INLINE void LoadF64(BCRegister reg, Float64 f64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::F64;
        value.m_value.d = f64;
    }

    HYP_FORCE_INLINE void LoadOffset(BCRegister reg, uint16_t offset)
    {
        // read value from stack at (sp - offset)
        // into the the register
        thread->m_regs[reg] = thread->m_stack[thread->m_stack.GetStackPointer() - offset];
    }

    HYP_FORCE_INLINE void LoadIndex(BCRegister reg, uint16_t index)
    {
        const auto &stk = state->MAIN_THREAD->m_stack;

        AssertThrowMsg(
            index < stk.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stk.GetStackPointer()
        );

        // read value from stack at the index into the the register
        // NOTE: read from main thread
        thread->m_regs[reg] = stk[index];
    }

    HYP_FORCE_INLINE void LoadStatic(BCRegister reg, uint16_t index)
    {
        // read value from static memory
        // at the index into the the register
        thread->m_regs[reg] = state->m_static_memory[index];
    }

    HYP_FORCE_INLINE void LoadString(BCRegister reg, uint32_t len, const char *str)
    {
        if (HeapValue *hv = state->HeapAlloc(thread)) {
            hv->Assign(VMString(str));

            // assign register value to the allocated object
            Value &sv = thread->m_regs[reg];
            sv.m_type = Value::HEAP_POINTER;
            sv.m_value.ptr = hv;

            hv->Mark();
        }
    }

    HYP_FORCE_INLINE void LoadAddr(BCRegister reg, BCAddress addr)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::ADDRESS;
        sv.m_value.addr = addr;
    }

    HYP_FORCE_INLINE void LoadFunc(BCRegister reg,
        BCAddress addr,
        uint8_t nargs,
        uint8_t flags)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::FUNCTION;
        sv.m_value.func.m_addr = addr;
        sv.m_value.func.m_nargs = nargs;
        sv.m_value.func.m_flags = flags;
    }

    HYP_FORCE_INLINE void LoadType(
        BCRegister reg,
        uint16_t type_name_len,
        const char *type_name,
        uint16_t size,
        char **names
    )
    {
        // allocate heap value
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);

        // create members
        Member *members = new Member[size];
        
        for (size_t i = 0; i < size; i++) {
            std::strncpy(members[i].name, names[i], 255);
            members[i].hash = hash_fnv_1(names[i]);
            members[i].value = Value(Value::HEAP_POINTER, {.ptr = nullptr});
        }

        // create prototype object
        hv->Assign(VMObject(members, size));

        delete[] members;

        // assign register value to the allocated object
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;

        hv->Mark();
    }

    HYP_FORCE_INLINE void LoadMem(BCRegister dst, BCRegister src, uint8_t index)
    {
        Value &sv = thread->m_regs[src];
        
        if (sv.m_type == Value::HEAP_POINTER) {
            HeapValue *hv = sv.m_value.ptr;
            if (hv == nullptr) {
                state->ThrowException(
                    thread,
                    Exception::NullReferenceException()
                );
                return;
            } else if (VMObject *obj_ptr = hv->GetPointer<VMObject>()) {
                AssertThrow(index < obj_ptr->GetSize());
                thread->m_regs[dst] = obj_ptr->GetMember(index).value;
                return;
            }
        }

        state->ThrowException(
            thread,
            Exception("Cannot access member by index: Not an VMObject")
        );
    }

    HYP_FORCE_INLINE void LoadMemHash(BCRegister dst_reg, BCRegister src_reg, uint32_t hash)
    {
        Value &sv = thread->m_regs[src_reg];

        if (sv.m_type == Value::HEAP_POINTER) {
            HeapValue *hv = sv.m_value.ptr;

            if (hv == nullptr) {
                state->ThrowException(
                    thread,
                    Exception::NullReferenceException()
                );
                return;
            } else if (VMObject *object = hv->GetPointer<VMObject>()) {
                if (Member *member = object->LookupMemberFromHash(hash)) {
                    thread->m_regs[dst_reg] = member->value;
                } else {
                    state->ThrowException(
                        thread,
                        Exception::MemberNotFoundException()
                    );
                }
                return;
            }
        }

        state->ThrowException(
            thread,
            Exception("Cannot access member by hash: Not an VMObject")
        );
    }

    HYP_FORCE_INLINE void LoadArrayIdx(BCRegister dst_reg, BCRegister src_reg, BCRegister index_reg)
    {
        Value &sv = thread->m_regs[src_reg];

        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Not an Array")
            );
            return;
        }

        HeapValue *ptr = sv.m_value.ptr;

        if (ptr == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );
            return;
        }

        struct {
            Number index;
        } key;

        if (!thread->m_regs[index_reg].GetSignedOrUnsigned(&key.index)) {
            state->ThrowException(
                thread,
                Exception("Array index must be of type Int or UInt")
            );
            return;
        }

        VMArray *array = ptr->GetPointer<VMArray>();

        if (array == nullptr) {
            if (auto *memory_buffer = ptr->GetPointer<VMMemoryBuffer>()) {
                // load the uint8 in the memory buffer, store it as int32
                Value memory_buffer_data;
                memory_buffer_data.m_type = Value::I32;

                if (key.index.flags & Number::FLAG_SIGNED) {
                    if (static_cast<SizeType>(key.index.i) >= memory_buffer->GetSize()) {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException()
                        );

                        return;
                    }

                    if (key.index.i < 0) {
                        // wrap around (python style)
                        key.index.i = static_cast<Int64>(memory_buffer->GetSize() + key.index.i);
                        if (key.index.i < 0 || static_cast<SizeType>(key.index.i) >= memory_buffer->GetSize()) {
                            state->ThrowException(
                                thread,
                                Exception::OutOfBoundsException()
                            );

                            return;
                        }
                    }

                    memory_buffer_data.m_value.i32 = static_cast<Int32>(static_cast<UInt8 *>(memory_buffer->GetBuffer())[key.index.i]);
                } else if (key.index.flags & Number::FLAG_UNSIGNED) {
                    if (static_cast<SizeType>(key.index.u) >= memory_buffer->GetSize()) {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException()
                        );

                        return;
                    }

                    memory_buffer_data.m_value.i32 = static_cast<Int32>(static_cast<UInt8 *>(memory_buffer->GetBuffer())[key.index.u]);
                }

                thread->m_regs[dst_reg] = memory_buffer_data;

                return;
            }

            state->ThrowException(
                thread,
                Exception("Not an Array or MemoryBuffer")
            );

            return;
        }

        if (key.index.flags & Number::FLAG_SIGNED) {
            if (static_cast<SizeType>(key.index.i) >= array->GetSize()) {
                state->ThrowException(
                    thread,
                    Exception::OutOfBoundsException()
                );
                return;
            }
            
            if (key.index.i < 0) {
                // wrap around (python style)
                key.index.i = (Int64)(array->GetSize() + key.index.i);
                if (key.index.i < 0 || (size_t)key.index.i >= array->GetSize()) {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException()
                    );
                    return;
                }
            }

            thread->m_regs[dst_reg] = array->AtIndex(key.index.i);
        } else if (key.index.flags & Number::FLAG_UNSIGNED) {
            if (static_cast<SizeType>(key.index.u) >= array->GetSize()) {
                state->ThrowException(
                    thread,
                    Exception::OutOfBoundsException()
                );
                return;
            }
            
            thread->m_regs[dst_reg] = array->AtIndex(key.index.u);
        }
    }

    HYP_FORCE_INLINE void LoadRef(BCRegister dst_reg, BCRegister src_reg)
    {
        Value &src = thread->m_regs[dst_reg];
        src.m_type = Value::VALUE_REF;
        src.m_value.value_ref = &thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void LoadDeref(BCRegister dst_reg, BCRegister src_reg)
    {
        Value &src = thread->m_regs[src_reg];
        AssertThrowMsg(src.m_type == Value::VALUE_REF, "Value type must be VALUE_REF in order to deref");
        AssertThrow(src.m_value.value_ref != nullptr);

        thread->m_regs[dst_reg] = *src.m_value.value_ref;
    }

    HYP_FORCE_INLINE void LoadNull(BCRegister reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = nullptr;
    }

    HYP_FORCE_INLINE void LoadTrue(BCRegister reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::BOOLEAN;
        sv.m_value.b = true;
    }

    HYP_FORCE_INLINE void LoadFalse(BCRegister reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::BOOLEAN;
        sv.m_value.b = false;
    }

    HYP_FORCE_INLINE void MovOffset(uint16_t offset, BCRegister reg)
    {
        // copy value from register to stack value at (sp - offset)
        thread->m_stack[thread->m_stack.GetStackPointer() - offset] =
            thread->m_regs[reg];
    }

    HYP_FORCE_INLINE void MovIndex(uint16_t index, BCRegister reg)
    {
        // copy value from register to stack value at index
        // NOTE: storing on main thread
        state->MAIN_THREAD->m_stack[index] = thread->m_regs[reg];
    }

    HYP_FORCE_INLINE void MovMem(BCRegister dst_reg, uint8_t index, BCRegister src_reg)
    {
        Value &sv = thread->m_regs[dst_reg];
        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by index: Not an VMObject")
            );
            return;
        }

        HeapValue *hv = sv.m_value.ptr;
        if (hv == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );
            return;
        }
        
        VMObject *object = hv->GetPointer<VMObject>();
        if (object == nullptr) {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by index: Not an VMObject")
            );
            return;
        }

        if (index >= object->GetSize()) {
            state->ThrowException(
                thread,
                Exception::OutOfBoundsException()
            );
            return;
        }
        
        object->GetMember(index).value = thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void MovMemHash(BCRegister dst_reg, uint32_t hash, BCRegister src_reg)
    {
        Value &sv = thread->m_regs[dst_reg];

        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by hash: Not an VMObject")
            );
            return;
        }

        HeapValue *hv = sv.m_value.ptr;
        if (hv == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );
            return;
        }

        VMObject *object = hv->GetPointer<VMObject>();
        if (object == nullptr) {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by hash: Not an VMObject")
            );
            return;
        }

        Member *member = object->LookupMemberFromHash(hash);
        if (member == nullptr) {
            state->ThrowException(
                thread,
                Exception::MemberNotFoundException()
            );
            return;
        }
        
        // set value in member
        member->value = thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void MovArrayIdx(BCRegister dst_reg, uint32_t index, BCRegister src_reg)
    {
        Value &sv = thread->m_regs[dst_reg];

        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Not an Array or MemoryBuffer")
            );
            return;
        }

        HeapValue *hv = sv.m_value.ptr;
        if (hv == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );
            return;
        }

        VMArray *array = hv->GetPointer<VMArray>();

        if (array == nullptr) {
            if (auto *memory_buffer = hv->GetPointer<VMMemoryBuffer>()) {
                if (static_cast<SizeType>(index) >= memory_buffer->GetSize()) {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException()
                    );

                    return;
                }

                if (index < 0) {
                    // wrap around (python style)
                    index = static_cast<Int64>(memory_buffer->GetSize() + static_cast<SizeType>(index));
                    if (index < 0 || static_cast<SizeType>(index) >= memory_buffer->GetSize()) {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException()
                        );
                        return;
                    }
                }

                Number dst_data;

                if (!thread->m_regs[src_reg].GetSignedOrUnsigned(&dst_data)) {
                    state->ThrowException(
                        thread,
                        Exception::InvalidArgsException("integer")
                    );

                    return;
                }

                Memory::Copy(&static_cast<UInt8 *>(memory_buffer->GetBuffer())[index], &dst_data, sizeof(dst_data));

                return;
            }

            state->ThrowException(
                thread,
                Exception("Not an Array or MemoryBuffer")
            );
            return;
        }

        if (index >= array->GetSize()) {
            state->ThrowException(
                thread,
                Exception::OutOfBoundsException()
            );
            return;
        }

        /*if (index < 0) {
            // wrap around (python style)
            index = array->GetSize() + index;
            if (index < 0 || index >= array->GetSize()) {
                state->ThrowException(
                    thread,
                    Exception::OutOfBoundsException()
                );
                return;
            }
        }*/
        
        array->AtIndex(index) = thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void MovArrayIdxReg(BCRegister dst_reg, BCRegister index_reg, BCRegister src_reg)
    {
        Value &sv = thread->m_regs[dst_reg];

        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Not an Array or MemoryBuffer")
            );
            return;
        }

        HeapValue *hv = sv.m_value.ptr;
        if (hv == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );
            return;
        }

        VMArray *array = hv->GetPointer<VMArray>();

        Number index;
        Value &index_register_value = thread->m_regs[index_reg];

        if (!index_register_value.GetSignedOrUnsigned(&index)) {
            state->ThrowException(
                thread,
                Exception::InvalidArgsException("integer")
            );

            return;
        }

        if (array == nullptr) {
            if (auto *memory_buffer = hv->GetPointer<VMMemoryBuffer>()) {
                Number dst_data;

                if (!thread->m_regs[src_reg].GetSignedOrUnsigned(&dst_data)) {
                    state->ThrowException(
                        thread,
                        Exception::InvalidArgsException("integer")
                    );

                    return;
                }

                if (index.flags & Number::FLAG_SIGNED) {
                    Int64 index_value = index.i;

                    if (static_cast<SizeType>(index_value) >= memory_buffer->GetSize()) {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException()
                        );

                        return;
                    }

                    if (index_value < 0) {
                        // wrap around (python style)
                        index_value = static_cast<Int64>(memory_buffer->GetSize() + static_cast<SizeType>(index_value));
                        if (index_value < 0 || static_cast<SizeType>(index_value) >= memory_buffer->GetSize()) {
                            state->ThrowException(
                                thread,
                                Exception::OutOfBoundsException()
                            );
                            return;
                        }
                    }
                    
                    Memory::Copy(&static_cast<UInt8 *>(memory_buffer->GetBuffer())[index_value], &dst_data, sizeof(dst_data));
                } else { // unsigned
                    const UInt64 index_value = index.u;

                    if (static_cast<SizeType>(index_value) >= memory_buffer->GetSize()) {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException()
                        );

                        return;
                    }

                    Memory::Copy(&static_cast<UInt8 *>(memory_buffer->GetBuffer())[index_value], &dst_data, sizeof(dst_data));
                }

                return;
            }

            state->ThrowException(
                thread,
                Exception("Not an Array or MemoryBuffer")
            );
            return;
        }

        if (index.flags & Number::FLAG_SIGNED) {
            Int64 index_value = index.i;

            if (static_cast<SizeType>(index_value) >= array->GetSize()) {
                state->ThrowException(
                    thread,
                    Exception::OutOfBoundsException()
                );

                return;
            }

            if (index_value < 0) {
                // wrap around (python style)
                index_value = static_cast<Int64>(array->GetSize() + static_cast<SizeType>(index_value));
                if (index_value < 0 || static_cast<SizeType>(index_value >= array->GetSize())) {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException()
                    );
                    return;
                }
            }

            array->AtIndex(index_value) = thread->m_regs[src_reg];
        } else { // unsigned
            const UInt64 index_value = index.u;

            if (static_cast<SizeType>(index_value) >= array->GetSize()) {
                state->ThrowException(
                    thread,
                    Exception::OutOfBoundsException()
                );

                return;
            }

            array->AtIndex(index_value) = thread->m_regs[src_reg];
        }
    }

    HYP_FORCE_INLINE void MovReg(BCRegister dst_reg, BCRegister src_reg)
    {
        thread->m_regs[dst_reg] = thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void HasMemHash(BCRegister dst_reg, BCRegister src_reg, uint32_t hash)
    {
        Value &src = thread->m_regs[src_reg];

        Value &dst = thread->m_regs[dst_reg];
        dst.m_type = Value::BOOLEAN;

        if (src.m_type == Value::HEAP_POINTER && src.m_value.ptr != nullptr) {
            if (VMObject *object = src.m_value.ptr->GetPointer<VMObject>()) {
                dst.m_value.b = (object->LookupMemberFromHash(hash) != nullptr);
                return;
            }
        }

        // not found, set it to false
        dst.m_value.b = false;
    }

    HYP_FORCE_INLINE void Push(BCRegister reg)
    {
        // push a copy of the register value to the top of the stack
        thread->m_stack.Push(thread->m_regs[reg]);
    }

    HYP_FORCE_INLINE void Pop()
    {
        thread->m_stack.Pop();
    }

    HYP_FORCE_INLINE void PopN(uint8_t n)
    {
        thread->m_stack.Pop(n);
    }

    HYP_FORCE_INLINE void PushArray(BCRegister dst_reg, BCRegister src_reg)
    {
        Value &dst = thread->m_regs[dst_reg];
        if (dst.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Not an Array")
            );
            return;
        }

        HeapValue *hv = dst.m_value.ptr;
        if (hv == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );
            return;
        }

        VMArray *array = hv->GetPointer<VMArray>();
        if (array == nullptr) {
            state->ThrowException(
                thread,
                Exception("Not an Array")
            );
            return;
        }

        array->Push(thread->m_regs[src_reg]);
    }

    HYP_FORCE_INLINE void Jmp(BCAddress addr)
    {
        bs->Seek(addr);
    }

    HYP_FORCE_INLINE void Je(BCAddress addr)
    {
        if (thread->m_regs.m_flags == EQUAL) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jne(BCAddress addr)
    {
        if (thread->m_regs.m_flags != EQUAL) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jg(BCAddress addr)
    {
        if (thread->m_regs.m_flags == GREATER) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jge(BCAddress addr)
    {
        if (thread->m_regs.m_flags == GREATER || thread->m_regs.m_flags == EQUAL) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Call(BCRegister reg, uint8_t nargs)
    {
        VM::Invoke(
            this,
            thread->m_regs[reg],
            nargs
        );
    }

    HYP_FORCE_INLINE void Ret()
    {
        // get top of stack (should be the address before jumping)
        Value &top = thread->GetStack().Top();
        AssertThrow(top.GetType() == Value::FUNCTION_CALL);
        
        // leave function and return to previous position
        bs->Seek(top.GetValue().call.return_address);

        // increase stack size by the amount required by the call
        thread->GetStack().m_sp += top.GetValue().call.varargs_push - 1;
        // NOTE: the -1 is because we will be popping the FUNCTION_CALL 
        // object from the stack anyway...

        // decrease function depth
        thread->m_func_depth--;
    }

    HYP_FORCE_INLINE void BeginTry(BCAddress addr)
    {
        ++thread->m_exception_state.m_try_counter;

        // increase stack size to store data about this try block
        Value info;
        info.m_type = Value::TRY_CATCH_INFO;
        info.m_value.try_catch_info.catch_address = addr;

        // store the info
        thread->m_stack.Push(info);
    }

    HYP_FORCE_INLINE void EndTry()
    {
        // pop the try catch info from the stack
        AssertThrow(thread->m_stack.Top().m_type == Value::TRY_CATCH_INFO);
        AssertThrow(thread->m_exception_state.m_try_counter != 0);

        // pop try catch info
        thread->m_stack.Pop();
        --thread->m_exception_state.m_try_counter;
    }

    HYP_FORCE_INLINE void New(BCRegister dst, BCRegister src)
    {
        // read value from register
        Value &proto_sv = thread->m_regs[src];
        AssertThrowMsg(proto_sv.m_type == Value::HEAP_POINTER, "NEW operand must be a pointer type (%d), got %d", Value::HEAP_POINTER, proto_sv.m_type);

        AssertThrow(proto_sv.m_value.ptr != nullptr);

        // the NEW instruction makes a copy of the $proto data member
        // of the prototype object.
        VMObject *proto_obj = proto_sv.m_value.ptr->GetPointer<VMObject>();
        AssertThrowMsg(proto_obj != nullptr, "NEW operand should be an VMObject");

        Member *proto_mem = proto_obj->LookupMemberFromHash(VMObject::PROTO_MEMBER_HASH);
        AssertThrow(proto_mem != nullptr);

        if (proto_mem->value.m_value.ptr == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );

            return;
        }

        Value &res = thread->m_regs[dst];

        if (proto_mem->value.m_type == Value::HEAP_POINTER) {
            if (VMObject *proto_mem_obj = proto_mem->value.m_value.ptr->GetPointer<VMObject>()) {
                // allocate heap value for new object
                HeapValue *hv = state->HeapAlloc(thread);
                AssertThrow(hv != nullptr);

                hv->Assign(VMObject(proto_mem_obj->GetMembers(), proto_mem_obj->GetSize(), proto_mem->value.m_value.ptr));

                res.m_type = Value::HEAP_POINTER;
                res.m_value.ptr = hv;

                hv->Mark();
            } else {
                state->ThrowException(
                    thread,
                    Exception::InvalidConstructorException()
                );
            }
        } else {
            // simply copy the value into the new value as it is not a heap pointer.
            res.m_type = proto_mem->value.m_type;
            res.m_value = proto_mem->value.m_value;
        }

        // assign destination to cloned value
        //state->CloneValue(proto_mem->value, thread, thread->m_regs[dst]);

        /*
        // allocate heap object
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);
        
        // create the VMObject from the proto
        hv->Assign(VMObject(proto_mem->value.m_value.ptr));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;*/
    }

    HYP_FORCE_INLINE void NewArray(BCRegister dst, uint32_t size)
    {
        // allocate heap object
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);

        hv->Assign(VMArray(size));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;

        hv->Mark();
    }

    HYP_FORCE_INLINE void Cmp(BCRegister lhs_reg, BCRegister rhs_reg)
    {
        // dropout early for comparing something against itself
        if (lhs_reg == rhs_reg) {
            thread->m_regs.m_flags = EQUAL;
            return;
        }

        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Number a, b;

        if (lhs->GetSignedOrUnsigned(&a) && rhs->GetSignedOrUnsigned(&b)) {
            if ((a.flags & Number::FLAG_SIGNED) && b.flags & Number::FLAG_SIGNED) {
                thread->m_regs.m_flags = (a.i == b.i)
                    ? EQUAL : ((a.i > b.i)
                    ? GREATER : NONE);
            } else if ((a.flags & Number::FLAG_SIGNED) && b.flags & Number::FLAG_UNSIGNED) {
                thread->m_regs.m_flags = (a.i == b.u)
                    ? EQUAL : ((a.i > b.u)
                    ? GREATER : NONE);
            } else if ((a.flags & Number::FLAG_UNSIGNED) && b.flags & Number::FLAG_SIGNED) {
                thread->m_regs.m_flags = (a.u == b.i)
                    ? EQUAL : ((a.u > b.i)
                    ? GREATER : NONE);
            } else if ((a.flags & Number::FLAG_UNSIGNED) && b.flags & Number::FLAG_UNSIGNED) {
                thread->m_regs.m_flags = (a.u == b.u)
                    ? EQUAL : ((a.u > b.u)
                    ? GREATER : NONE);
            }
        } else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f)) {
            thread->m_regs.m_flags = (a.f == b.f)
                ? EQUAL : ((a.f > b.f)
                ? GREATER : NONE);
        } else if (lhs->m_type == Value::BOOLEAN && rhs->m_type == Value::BOOLEAN) {
            thread->m_regs.m_flags = (lhs->m_value.b == rhs->m_value.b)
                ? EQUAL : ((lhs->m_value.b > rhs->m_value.b)
                ? GREATER : NONE);
        } else if (lhs->m_type == Value::HEAP_POINTER && rhs->m_type == Value::HEAP_POINTER) {
            const auto res = Value::CompareAsPointers(lhs, rhs);

            if (res != -1) {
                thread->m_regs.m_flags = res;
            } else {
                state->ThrowException(
                    thread,
                    Exception::InvalidComparisonException(
                        lhs->GetTypeString(),
                        rhs->GetTypeString()
                    )
                );
            }
        } else if (lhs->m_type == Value::FUNCTION && rhs->m_type == Value::FUNCTION) {
            thread->m_regs.m_flags = Value::CompareAsFunctions(lhs, rhs);
        } else if (lhs->m_type == Value::NATIVE_FUNCTION && rhs->m_type == Value::NATIVE_FUNCTION) {
            thread->m_regs.m_flags = Value::CompareAsNativeFunctions(lhs, rhs);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidComparisonException(
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
        }
    }

    HYP_FORCE_INLINE void CmpZ(BCRegister reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[reg];

        Number num;

        if (lhs->GetSignedOrUnsigned(&num)) {
            thread->m_regs.m_flags = ((num.flags & Number::FLAG_SIGNED) ? !num.i : !num.u) ? EQUAL : NONE;
        } else if (lhs->GetFloatingPoint(&num.f)) {
            thread->m_regs.m_flags = !num.f ? EQUAL : NONE;
        } else if (lhs->m_type == Value::BOOLEAN) {
            thread->m_regs.m_flags = !lhs->m_value.b ? EQUAL : NONE;
        } else if (lhs->m_type == Value::HEAP_POINTER) {
            thread->m_regs.m_flags = !lhs->m_value.ptr ? EQUAL : NONE;
        } else if (lhs->m_type == Value::FUNCTION) {
            // functions are never null
            thread->m_regs.m_flags = NONE;
        } else {
            char buffer[256];
            std::sprintf(
                buffer,
                "Cannot determine if type '%s' is non-zero",
                lhs->GetTypeString()
            );
            state->ThrowException(
                thread,
                Exception(buffer)
            );
        }
    }

    HYP_FORCE_INLINE void Add(BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION(a, b, +);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "ADD",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Sub(BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION(a, b, -);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SUB",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Mul(BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION(a, b, *);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "MUL",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Div(BCRegister lhs_reg, BCRegister rhs_reg, BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            switch (result.m_type) {
            case Value::I32:
                if (b.i == 0) {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i32 = static_cast<Int32>(a.i / b.i);
                break;
            case Value::I64:
                if (b.i == 0) {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i64 = a.i / b.i;
                break;
            case Value::U32:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.u32 = static_cast<UInt32>(a.i);
                } else {
                    result.m_value.u32 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 / static_cast<UInt32>(b.i);
                } else {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 / static_cast<UInt32>(b.u);
                }
                break;
            case Value::U64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.u64 = static_cast<UInt64>(a.i);
                } else {
                    result.m_value.u64 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 / static_cast<UInt32>(b.i);
                } else {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 / b.u;
                }
                break;
            case Value::F32:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.f = static_cast<Float32>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.f = static_cast<Float32>(a.u);
                } else {
                    result.m_value.f = static_cast<Float32>(a.f);
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    result.m_value.f = result.m_value.f / static_cast<Float32>(b.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.f = result.m_value.f / static_cast<Float32>(b.u);
                } else {
                    result.m_value.f = result.m_value.f / static_cast<Float32>(b.f);
                }
                break;
            case Value::F64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.d = static_cast<Float64>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.d = static_cast<Float64>(a.u);
                } else {
                    result.m_value.d = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    result.m_value.d = result.m_value.d / static_cast<Float64>(b.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.d = result.m_value.d / static_cast<Float64>(b.u);
                } else {
                    result.m_value.d = result.m_value.d / b.f;
                }
                break;
            default:
                AssertThrowMsg(false, "Result type was not a number. Investigate");
                break;
            }
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "DIV",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Mod(BCRegister lhs_reg, BCRegister rhs_reg, BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            switch (result.m_type) {
            case Value::I32:
                if (b.i == 0) {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i32 = static_cast<Int32>(a.i % b.i);
                break;
            case Value::I64:
                if (b.i == 0) {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i64 = a.i % b.i;
                break;
            case Value::U32:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.u32 = static_cast<UInt32>(a.i);
                } else {
                    result.m_value.u32 = static_cast<UInt32>(a.u);
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 % static_cast<UInt32>(b.i);
                } else {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 % static_cast<UInt32>(b.u);
                }
                break;
            case Value::U64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.u64 = static_cast<UInt64>(a.i);
                } else {
                    result.m_value.u64 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 % static_cast<UInt32>(b.i);
                } else {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 % b.u;
                }
                break;
            case Value::F32:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.f = static_cast<Float32>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.f = static_cast<Float32>(a.u);
                } else {
                    result.m_value.f = static_cast<Float32>(a.f);
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    result.m_value.f = fmodf(result.m_value.f, static_cast<Float32>(b.i));
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.f = fmodf(result.m_value.f, static_cast<Float32>(b.u));
                } else {
                    result.m_value.f = fmodf(result.m_value.f, a.f);
                }
                break;
            case Value::F64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.d = static_cast<Float64>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.d = static_cast<Float64>(a.u);
                } else {
                    result.m_value.d = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    result.m_value.d = std::fmod(result.m_value.d, static_cast<Float64>(b.i));
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.d = std::fmod(result.m_value.d, static_cast<Float64>(b.u));
                } else {
                    result.m_value.d = std::fmod(result.m_value.d, a.f);
                }
                break;
            default:
                AssertThrowMsg(false, "Invalid type, should not reach this state.");
                break;
            }
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "MOD",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void And(BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, &);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "AND",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Or(
        BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg
    )
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, |);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "OR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Xor(
        BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg
    )
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, ^);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "XOR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Shl(BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, <<);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SHL",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Shr(BCRegister lhs_reg,
        BCRegister rhs_reg,
        BCRegister dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b)) {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, >>);
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SHR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()
                )
            );
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dst_reg] = result;
    }

    HYP_FORCE_INLINE void Not(BCRegister reg)
    {
        // load value from register
        Value *value = &thread->m_regs[reg];

        union {
            Int64 i;
            UInt64 u;
        };

        if (value->GetInteger(&i)) {
            if (value->m_type == Value::I32) {
                value->m_value.i32 = static_cast<Int32>(~i);
            } else {
                value->m_value.i64 = ~i;
            }
        } else if (value->GetUnsigned(&u)) {
            if (value->m_type == Value::U32) {
                value->m_value.u32 = static_cast<UInt32>(~u);
            } else {
                value->m_value.u64 = ~u;
            }
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidBitwiseArgument()
            );
            return;
        }
    }

    HYP_FORCE_INLINE void Throw(BCRegister reg)
    {
        // load value from register
        Value *value = &thread->m_regs[reg];

        state->ThrowException(
            thread,
            Exception("User exception")
        );
    }

    HYP_FORCE_INLINE void ExportSymbol(BCRegister reg, uint32_t hash)
    {
        if (!state->GetExportedSymbols().Store(hash, thread->m_regs[reg]).second) {
            state->ThrowException(
                thread,
                Exception::DuplicateExportException()
            );
        }
    }

    HYP_FORCE_INLINE void Neg(BCRegister reg)
    {
        // load value from register
        Value *value = &thread->m_regs[reg];

        union {
            Int64 i;
            UInt64 u;
            Float64 f;
        };

        if (value->GetInteger(&i)) {
            if (value->m_type == Value::I32) {
                value->m_value.i32 = static_cast<Int32>(-i);
            } else {
                value->m_value.i64 = -i;
            }
        } else if (value->GetUnsigned(&u)) {
            if (value->m_type == Value::U32) {
                value->m_value.u32 = static_cast<UInt32>(-u);
            } else {
                value->m_value.u64 = -u;
            }
        } else if (value->GetFloatingPoint(&f)) {
            if (value->m_type == Value::F32) {
                value->m_value.f = static_cast<Float32>(-f);
            } else {
                value->m_value.d = -f;
            }
        } else {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "NEG",
                    value->GetTypeString()
                )
            );
            return;
        }
    }
};

} // namespace vm
} // namespace hyperion

#endif
