#ifndef INSTRUCTION_HANDLER_HPP
#define INSTRUCTION_HANDLER_HPP

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/Object.hpp>
#include <script/vm/ImmutableString.hpp>
#include <script/vm/TypeInfo.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <script/Typedefs.hpp>
#include <script/Hasher.hpp>

#include <stdint.h>
#include <cstring>

#define HYP_NUMERIC_OPERATION(a, b, oper) \
    do { \
        switch (result.m_type) { \
            case Value::I32: \
                result.m_value.i32 = a.i oper b.i; \
                break; \
            case Value::I64: \
                result.m_value.i64 = a.i oper b.i; \
                break; \
            case Value::U32: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 = static_cast<auint32>(a.i); \
                } else { \
                    result.m_value.u32 = a.u; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 oper##= static_cast<auint32>(b.i); \
                } else { \
                    result.m_value.u32 oper##= b.u; \
                } \
                break; \
            case Value::U64: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 = static_cast<auint64>(a.i); \
                } else { \
                    result.m_value.u64 = a.u; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 oper##= static_cast<auint32>(b.i); \
                } else { \
                    result.m_value.u64 oper##= b.u; \
                } \
                break; \
            case Value::F32: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.f = static_cast<afloat32>(a.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.f = static_cast<afloat32>(a.u); \
                } else { \
                    result.m_value.f = a.f; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.f oper##= static_cast<afloat32>(b.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.f oper##= static_cast<afloat32>(b.u); \
                } else { \
                    result.m_value.f oper##= a.f; \
                } \
                break; \
            case Value::F64: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.d = static_cast<afloat64>(a.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.d = static_cast<afloat64>(a.u); \
                } else { \
                    result.m_value.d = a.f; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.d oper##= static_cast<afloat64>(b.i); \
                } else if (a.flags & Number::FLAG_UNSIGNED) { \
                    result.m_value.d oper##= static_cast<afloat64>(b.u); \
                } else { \
                    result.m_value.d oper##= a.f; \
                } \
                break; \
        } \
    } while (0)

#define HYP_NUMERIC_OPERATION_BITWISE(a, b, oper) \
    do { \
        switch (result.m_type) { \
            case Value::I32: \
                result.m_value.i32 = a.i oper b.i; \
                break; \
            case Value::I64: \
                result.m_value.i64 = a.i oper b.i; \
                break; \
            case Value::U32: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 = static_cast<auint32>(a.i); \
                } else { \
                    result.m_value.u32 = a.u; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u32 oper##= static_cast<auint32>(b.i); \
                } else { \
                    result.m_value.u32 oper##= b.u; \
                } \
                break; \
            case Value::U64: \
                if (a.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 = static_cast<auint64>(a.i); \
                } else { \
                    result.m_value.u64 = a.u; \
                } \
                if (b.flags & Number::FLAG_SIGNED) { \
                    result.m_value.u64 oper##= static_cast<auint32>(b.i); \
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

struct InstructionHandler {
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

    inline void StoreStaticString(uint32_t len, const char *str)
    {
        // the value will be freed on
        // the destructor call of state->m_static_memory
        HeapValue *hv = new HeapValue();
        hv->Assign(ImmutableString(str));

        Value sv;
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;

        state->m_static_memory.Store(std::move(sv));
    }

    inline void StoreStaticAddress(bc_address_t addr)
    {
        Value sv;
        sv.m_type = Value::ADDRESS;
        sv.m_value.addr = addr;

        state->m_static_memory.Store(std::move(sv));
    }

    inline void StoreStaticFunction(bc_address_t addr,
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

    inline void StoreStaticType(const char *type_name,
        uint16_t size,
        char **names)
    {
        // the value will be freed on
        // the destructor call of state->m_static_memory
        HeapValue *hv = new HeapValue();
        hv->Assign(TypeInfo(type_name, size, names));

        Value sv;
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;
        state->m_static_memory.Store(std::move(sv));
    }

    inline void LoadI32(bc_reg_t reg, aint32 i32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::I32;
        value.m_value.i32 = i32;
    }

    inline void LoadI64(bc_reg_t reg, aint64 i64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::I64;
        value.m_value.i64 = i64;
    }

    inline void LoadU32(bc_reg_t reg, auint32 u32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::U32;
        value.m_value.u32 = u32;
    }

    inline void LoadU64(bc_reg_t reg, auint64 u64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::U64;
        value.m_value.u64 = u64;
    }

    inline void LoadF32(bc_reg_t reg, afloat32 f32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::F32;
        value.m_value.f = f32;
    }

    inline void LoadF64(bc_reg_t reg, afloat64 f64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::F64;
        value.m_value.d = f64;
    }

    inline void LoadOffset(bc_reg_t reg, uint16_t offset)
    {
        // read value from stack at (sp - offset)
        // into the the register
        thread->m_regs[reg] = thread->m_stack[thread->m_stack.GetStackPointer() - offset];
    }

    inline void LoadIndex(bc_reg_t reg, uint16_t index)
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

    inline void LoadStatic(bc_reg_t reg, uint16_t index)
    {
        // read value from static memory
        // at the index into the the register
        thread->m_regs[reg] = state->m_static_memory[index];
    }

    inline void LoadString(bc_reg_t reg, uint32_t len, const char *str)
    {
        if (HeapValue *hv = state->HeapAlloc(thread)) {
            hv->Assign(ImmutableString(str));

            // assign register value to the allocated object
            Value &sv = thread->m_regs[reg];
            sv.m_type = Value::HEAP_POINTER;
            sv.m_value.ptr = hv;

            hv->Mark();
        }
    }

    inline void LoadAddr(bc_reg_t reg, bc_address_t addr)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::ADDRESS;
        sv.m_value.addr = addr;
    }

    inline void LoadFunc(bc_reg_t reg,
        bc_address_t addr,
        uint8_t nargs,
        uint8_t flags)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::FUNCTION;
        sv.m_value.func.m_addr = addr;
        sv.m_value.func.m_nargs = nargs;
        sv.m_value.func.m_flags = flags;
    }

    inline void LoadType(bc_reg_t reg,
        uint16_t type_name_len,
        const char *type_name,
        uint16_t size,
        char **names)
    {
        // allocate heap value
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);

        // create members
        Member *members = new Member[size];
        
        for (size_t i = 0; i < size; i++) {
            std::strncpy(members[i].name, names[i], 255);
            members[i].hash = hash_fnv_1(names[i]);
        }

        // create prototype object
        hv->Assign(Object(members, size));

        delete[] members;

        // assign register value to the allocated object
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;

        hv->Mark();
    }

    inline void LoadMem(bc_reg_t dst, bc_reg_t src, uint8_t index)
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
            } else if (Object *obj_ptr = hv->GetPointer<Object>()) {
                AssertThrow(index < obj_ptr->GetSize());
                thread->m_regs[dst] = obj_ptr->GetMember(index).value;
                return;
            }
        }

        state->ThrowException(
            thread,
            Exception("Not an Object")
        );
    }

    inline void LoadMemHash(bc_reg_t dst_reg, bc_reg_t src_reg, uint32_t hash)
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
            } else if (Object *object = hv->GetPointer<Object>()) {
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
            Exception("Not an Object")
        );
    }

    inline void LoadArrayIdx(bc_reg_t dst_reg, bc_reg_t src_reg, bc_reg_t index_reg)
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

        union {
            aint64 index;
            ImmutableString *str;
        } key;

        if (!thread->m_regs[index_reg].GetInteger(&key.index)) {
            state->ThrowException(
                thread,
                Exception("Array index must be of type Int or String")
            );
            return;
        }

        Array *array = ptr->GetPointer<Array>();
        if (array == nullptr) {
            state->ThrowException(
                thread,
                Exception("Not an Array")
            );
            return;
        }
        
        if ((size_t)key.index >= array->GetSize()) {
            state->ThrowException(
                thread,
                Exception::OutOfBoundsException()
            );
            return;
        }
        
        if (key.index < 0) {
            // wrap around (python style)
            key.index = (aint64)(array->GetSize() + key.index);
            if (key.index < 0 || (size_t)key.index >= array->GetSize()) {
                state->ThrowException(
                    thread,
                    Exception::OutOfBoundsException()
                );
                return;
            }
        }

        thread->m_regs[dst_reg] = array->AtIndex(key.index);
    }

    inline void LoadRef(bc_reg_t dst_reg, bc_reg_t src_reg)
    {
        Value &src = thread->m_regs[dst_reg];
        src.m_type = Value::VALUE_REF;
        src.m_value.value_ref = &thread->m_regs[src_reg];
    }

    inline void LoadDeref(bc_reg_t dst_reg, bc_reg_t src_reg)
    {
        Value &src = thread->m_regs[src_reg];
        AssertThrowMsg(src.m_type == Value::VALUE_REF, "Value type must be VALUE_REF in order to deref");
        AssertThrow(src.m_value.value_ref != nullptr);

        thread->m_regs[dst_reg] = *src.m_value.value_ref;
    }

    inline void LoadNull(bc_reg_t reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = nullptr;
    }

    inline void LoadTrue(bc_reg_t reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::BOOLEAN;
        sv.m_value.b = true;
    }

    inline void LoadFalse(bc_reg_t reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::BOOLEAN;
        sv.m_value.b = false;
    }

    inline void MovOffset(uint16_t offset, bc_reg_t reg)
    {
        // copy value from register to stack value at (sp - offset)
        thread->m_stack[thread->m_stack.GetStackPointer() - offset] =
            thread->m_regs[reg];
    }

    inline void MovIndex(uint16_t index, bc_reg_t reg)
    {
        // copy value from register to stack value at index
        // NOTE: storing on main thread
        state->MAIN_THREAD->m_stack[index] = thread->m_regs[reg];
    }

    inline void MovMem(bc_reg_t dst_reg, uint8_t index, bc_reg_t src_reg)
    {
        Value &sv = thread->m_regs[dst_reg];
        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Not an Object")
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
        
        Object *object = hv->GetPointer<Object>();
        if (object == nullptr) {
            state->ThrowException(
                thread,
                Exception("Not an Object")
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

    inline void MovMemHash(bc_reg_t dst_reg, uint32_t hash, bc_reg_t src_reg)
    {
        Value &sv = thread->m_regs[dst_reg];

        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Not an Object")
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

        Object *object = hv->GetPointer<Object>();
        if (object == nullptr) {
            state->ThrowException(
                thread,
                Exception("Not an Object")
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

    inline void MovArrayIdx(bc_reg_t dst_reg, uint32_t index, bc_reg_t src_reg)
    {
        Value &sv = thread->m_regs[dst_reg];

        if (sv.m_type != Value::HEAP_POINTER) {
            state->ThrowException(
                thread,
                Exception("Not an Array")
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

        Array *array = hv->GetPointer<Array>();
        if (array == nullptr) {
            state->ThrowException(
                thread,
                Exception("Not an Array")
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

    inline void MovReg(bc_reg_t dst_reg, bc_reg_t src_reg)
    {
        thread->m_regs[dst_reg] = thread->m_regs[src_reg];
    }

    inline void HasMemHash(bc_reg_t dst_reg, bc_reg_t src_reg, uint32_t hash)
    {
        Value &src = thread->m_regs[src_reg];

        Value &dst = thread->m_regs[dst_reg];
        dst.m_type = Value::BOOLEAN;

        if (src.m_type == Value::HEAP_POINTER && src.m_value.ptr != nullptr) {
            if (Object *object = src.m_value.ptr->GetPointer<Object>()) {
                dst.m_value.b = (object->LookupMemberFromHash(hash) != nullptr);
                return;
            }
        }

        // not found, set it to false
        dst.m_value.b = false;
    }

    inline void Push(bc_reg_t reg)
    {
        // push a copy of the register value to the top of the stack
        thread->m_stack.Push(thread->m_regs[reg]);
    }

    inline void Pop()
    {
        thread->m_stack.Pop();
    }

    inline void PopN(uint8_t n)
    {
        thread->m_stack.Pop(n);
    }

    inline void PushArray(bc_reg_t dst_reg, bc_reg_t src_reg)
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

        Array *array = hv->GetPointer<Array>();
        if (array == nullptr) {
            state->ThrowException(
                thread,
                Exception("Not an Array")
            );
            return;
        }

        array->Push(thread->m_regs[src_reg]);
    }

    inline void Echo(bc_reg_t reg)
    {
        VM::Print(thread->m_regs[reg]);
    }

    inline void EchoNewline()
    {
        utf::fputs(UTF8_CSTR("\n"), stdout);
    }

    inline void Jmp(bc_address_t addr)
    {
        bs->Seek(addr);
    }

    inline void Je(bc_address_t addr)
    {
        if (thread->m_regs.m_flags == EQUAL) {
            bs->Seek(addr);
        }
    }

    inline void Jne(bc_address_t addr)
    {
        if (thread->m_regs.m_flags != EQUAL) {
            bs->Seek(addr);
        }
    }

    inline void Jg(bc_address_t addr)
    {
        if (thread->m_regs.m_flags == GREATER) {
            bs->Seek(addr);
        }
    }

    inline void Jge(bc_address_t addr)
    {
        if (thread->m_regs.m_flags == GREATER || thread->m_regs.m_flags == EQUAL) {
            bs->Seek(addr);
        }
    }

    inline void Call(bc_reg_t reg, uint8_t nargs)
    {
        VM::Invoke(
            this,
            thread->m_regs[reg],
            nargs
        );
    }

    inline void Ret()
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

    inline void BeginTry(bc_address_t addr)
    {
        thread->m_exception_state.m_try_counter++;

        // increase stack size to store data about this try block
        Value info;
        info.m_type = Value::TRY_CATCH_INFO;
        info.m_value.try_catch_info.catch_address = addr;

        // store the info
        thread->m_stack.Push(info);
    }

    inline void EndTry()
    {
        // pop the try catch info from the stack
        AssertThrow(thread->m_stack.Top().m_type == Value::TRY_CATCH_INFO);
        AssertThrow(thread->m_exception_state.m_try_counter > 0);

        // pop try catch info
        thread->m_stack.Pop();
        thread->m_exception_state.m_try_counter--;
    }

    inline void New(bc_reg_t dst, bc_reg_t src)
    {
        // read value from register
        Value &proto_sv = thread->m_regs[src];
        AssertThrowMsg(proto_sv.m_type == Value::HEAP_POINTER, "NEW operand must be a pointer type (%d), got %d", Value::HEAP_POINTER, proto_sv.m_type);

        // the NEW instruction makes a copy of the $proto data member
        // of the prototype object.
        Object *proto_obj = proto_sv.m_value.ptr->GetPointer<Object>();
        AssertThrowMsg(proto_obj != nullptr, "NEW operand should be an Object");

        Member *proto_mem = proto_obj->LookupMemberFromHash(Object::PROTO_MEMBER_HASH);
        AssertThrow(proto_mem != nullptr);

        if (proto_mem->value.m_value.ptr == nullptr) {
            state->ThrowException(
                thread,
                Exception::NullReferenceException()
            );
        }

        Value &res = thread->m_regs[dst];

        if (proto_mem->value.m_type == Value::HEAP_POINTER) {
            if (Object *proto_mem_obj = proto_mem->value.m_value.ptr->GetPointer<Object>()) {
                // allocate heap value for new object
                HeapValue *hv = state->HeapAlloc(thread);
                AssertThrow(hv != nullptr);

                hv->Assign(Object(proto_mem_obj->GetMembers(), proto_mem_obj->GetSize(), proto_sv.m_value.ptr));

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
        
        // create the Object from the proto
        hv->Assign(Object(proto_mem->value.m_value.ptr));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;*/
    }

    inline void NewArray(bc_reg_t dst, uint32_t size)
    {
        // allocate heap object
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);

        hv->Assign(Array(size));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;

        hv->Mark();
    }

    inline void Cmp(bc_reg_t lhs_reg, bc_reg_t rhs_reg)
    {
        // dropout early for comparing something against itself
        if (lhs_reg == rhs_reg) {
            thread->m_regs.m_flags = EQUAL;
            return;
        }

        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        union {
            aint64 i;
            auint64 u;
            afloat64 f;
        } a, b;

        if (lhs->GetUnsigned(&a.u) && rhs->GetUnsigned(&b.u)) {
            thread->m_regs.m_flags = (a.u == b.u)
                ? EQUAL : ((a.u > b.u)
                ? GREATER : NONE);
        } else if (lhs->GetInteger(&a.i) && rhs->GetInteger(&b.i)) {
            thread->m_regs.m_flags = (a.i == b.i)
                ? EQUAL : ((a.i > b.i)
                ? GREATER : NONE);
        } else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f)) {
            thread->m_regs.m_flags = (a.f == b.f)
                ? EQUAL : ((a.f > b.f)
                ? GREATER : NONE);
        } else if (lhs->m_type == Value::BOOLEAN && rhs->m_type == Value::BOOLEAN) {
            thread->m_regs.m_flags = (lhs->m_value.b == rhs->m_value.b)
                ? EQUAL : ((lhs->m_value.b > rhs->m_value.b)
                ? GREATER : NONE);
        } else if (lhs->m_type == Value::HEAP_POINTER && rhs->m_type == Value::HEAP_POINTER) {
            int res = VM::CompareAsPointers(lhs, rhs);
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
            thread->m_regs.m_flags = VM::CompareAsFunctions(lhs, rhs);
        } else if (lhs->m_type == Value::NATIVE_FUNCTION && rhs->m_type == Value::NATIVE_FUNCTION) {
            thread->m_regs.m_flags = VM::CompareAsNativeFunctions(lhs, rhs);
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

    inline void CmpZ(bc_reg_t reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[reg];

        union {
            aint64 i;
            afloat64 f;
        };

        if (lhs->GetInteger(&i)) {
            thread->m_regs.m_flags = !i ? EQUAL : NONE;
        } else if (lhs->GetFloatingPoint(&f)) {
            thread->m_regs.m_flags = !f ? EQUAL : NONE;
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

    inline void Add(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Sub(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Mul(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Div(bc_reg_t lhs_reg, bc_reg_t rhs_reg, bc_reg_t dst_reg)
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
                result.m_value.i32 = a.i / b.i;
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
                    result.m_value.u32 = static_cast<auint32>(a.i);
                } else {
                    result.m_value.u32 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 / static_cast<auint32>(b.i);
                } else {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 / b.u;
                }
                break;
            case Value::U64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.u64 = static_cast<auint64>(a.i);
                } else {
                    result.m_value.u64 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 / static_cast<auint32>(b.i);
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
                    result.m_value.f = static_cast<afloat32>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.f = static_cast<afloat32>(a.u);
                } else {
                    result.m_value.f = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    result.m_value.f = result.m_value.f / static_cast<afloat32>(b.i);
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.f = result.m_value.f / static_cast<afloat32>(b.u);
                } else {
                    if (b.f == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.f = result.m_value.f / a.f;
                }
                break;
            case Value::F64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.d = static_cast<afloat64>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.d = static_cast<afloat64>(a.u);
                } else {
                    result.m_value.d = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.d = result.m_value.d / static_cast<afloat64>(b.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.d = result.m_value.d / static_cast<afloat64>(b.u);
                } else {
                    if (b.f == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.d = result.m_value.d / a.f;
                }
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

    inline void Mod(bc_reg_t lhs_reg, bc_reg_t rhs_reg, bc_reg_t dst_reg)
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
                result.m_value.i32 = a.i % b.i;
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
                    result.m_value.u32 = static_cast<auint32>(a.i);
                } else {
                    result.m_value.u32 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 % static_cast<auint32>(b.i);
                } else {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 % b.u;
                }
                break;
            case Value::U64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.u64 = static_cast<auint64>(a.i);
                } else {
                    result.m_value.u64 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 % static_cast<auint32>(b.i);
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
                    result.m_value.f = static_cast<afloat32>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.f = static_cast<afloat32>(a.u);
                } else {
                    result.m_value.f = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.f = std::fmodf(result.m_value.f, static_cast<afloat32>(b.i));
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.f = std::fmodf(result.m_value.f, static_cast<afloat32>(b.u));
                } else {
                    if (b.f == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.f = std::fmodf(result.m_value.f, a.f);
                }
                break;
            case Value::F64:
                if (a.flags & Number::FLAG_SIGNED) {
                    result.m_value.d = static_cast<afloat64>(a.i);
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    result.m_value.d = static_cast<afloat64>(a.u);
                } else {
                    result.m_value.d = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED) {
                    if (b.i == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.d = std::fmod(result.m_value.d, static_cast<afloat64>(b.i));
                } else if (a.flags & Number::FLAG_UNSIGNED) {
                    if (b.u == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.d = std::fmod(result.m_value.d, static_cast<afloat64>(b.u));
                } else {
                    if (b.f == 0) {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.d = std::fmod(result.m_value.d, a.f);
                }
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

    inline void And(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Or(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Xor(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Shl(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Shr(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
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

    inline void Not(bc_reg_t reg)
    {
        // load value from register
        Value *value = &thread->m_regs[reg];

        union {
            aint64 i;
            auint64 u;
        };

        if (value->GetInteger(&i)) {
            if (value->m_type == Value::I32) {
                value->m_value.i32 = ~i;
            } else {
                value->m_value.i64 = ~i;
            }
        } else if (value->GetUnsigned(&u)) {
            if (value->m_type == Value::U32) {
                value->m_value.u32 = ~u;
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

    inline void Neg(bc_reg_t reg)
    {
        // load value from register
        Value *value = &thread->m_regs[reg];

        union {
            aint64 i;
            auint64 u;
            afloat64 f;
        };

        if (value->GetInteger(&i)) {
            if (value->m_type == Value::I32) {
                value->m_value.i32 = -i;
            } else {
                value->m_value.i64 = -i;
            }
        } else if (value->GetUnsigned(&u)) {
            if (value->m_type == Value::U32) {
                value->m_value.u32 = -u;
            } else {
                value->m_value.u64 = -u;
            }
        } else if (value->GetFloatingPoint(&f)) {
            if (value->m_type == Value::F32) {
                value->m_value.f = -f;
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

    #if 0
    inline void StoreStaticString(uint32_t len, const char *str);
    void StoreStaticAddress(bc_address_t addr);
    void StoreStaticFunction(bc_address_t addr,
        uint8_t nargs, uint8_t is_variadic);
    void StoreStaticType(const char *type_name,
        uint16_t size, char **names);
    void LoadI32(bc_reg_t reg, aint32 i32);
    void LoadI64(bc_reg_t reg, aint64 i64);
    void LoadF32(bc_reg_t reg, afloat32 f32);
    void LoadF64(bc_reg_t reg, afloat64 f64);
    void LoadOffset(bc_reg_t reg, uint8_t offset);
    void LoadIndex(bc_reg_t reg, uint16_t index);
    void LoadStatic(bc_reg_t reg, uint16_t index);
    void LoadString(bc_reg_t reg, uint32_t len, const char *str);
    void LoadAddr(bc_reg_t reg, uint8_t addr);
    void LoadFunc(bc_reg_t reg, bc_address_t addr,
        uint8_t nargs, uint8_t is_variadic);
    void LoadType(bc_reg_t reg, uint16_t type_name_len,
        const char *type_name, uint16_t size, char **names);
    void LoadMem(uint8_t dst, uint8_t src, uint8_t index);
    void LoadMemHash(uint8_t dst, uint8_t src, uint32_t hash);
    void LoadArrayIdx(uint8_t dst, uint8_t src, uint8_t index_reg);
    void LoadNull(bc_reg_t reg);
    void LoadTrue(bc_reg_t reg);
    void LoadFalse(bc_reg_t reg);
    void MovOffset(uint16_t offset, bc_reg_t reg);
    void MovIndex(uint16_t index, bc_reg_t reg);
    void MovMem(uint8_t dst, uint8_t index, uint8_t src);
    void MovMemHash(uint8_t dst, uint32_t hash, uint8_t src);
    void MovArrayIdx(uint8_t dst, uint32_t index, uint8_t src);
    void MovReg(uint8_t dst, uint8_t src);
    void HashMemHash(uint8_t dst, uint8_t src, uint32_t hash);
    void Push(bc_reg_t reg);
    void Pop(ExecutionThread *thread);
    void PopN(uint8_t n);
    void PushArray(uint8_t dst, uint8_t src);
    void Echo(bc_reg_t reg);
    void EchoNewline(ExecutionThread *thread);
    void Jmp(bc_reg_t reg);
    void Je(bc_reg_t reg);
    void Jne(bc_reg_t reg);
    void Jg(bc_reg_t reg);
    void Jge(bc_reg_t reg);
    void Call(bc_reg_t reg, uint8_t nargs);
    void Ret(ExecutionThread *thread);
    void BeginTry(bc_reg_t reg);
    void EndTry(ExecutionThread *thread);
    void New(uint8_t dst, uint8_t src);
    void NewArray(uint8_t dst, uint32_t size);
    void Cmp(uint8_t lhs_reg, uint8_t rhs_reg);
    void CmpZ(bc_reg_t reg);
    void Add(uint8_t lhs_reg, uint8_t rhs_reg, uint8_t dst_reg);
    void Sub(uint8_t lhs_reg, uint8_t rhs_reg, uint8_t dst_reg);
    void Mul(uint8_t lhs_reg, uint8_t rhs_reg, uint8_t dst_reg);
    void Div(uint8_t lhs_reg, uint8_t rhs_reg, uint8_t dst_reg);
    void Neg(bc_reg_t reg);

    #endif
};

} // namespace vm
} // namespace hyperion

#endif
