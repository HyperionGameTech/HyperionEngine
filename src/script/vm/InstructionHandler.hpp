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
#include <script/Typedefs.hpp>

#include <system/debug.h>

#include <util/defines.h>

#include <stdint.h>

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

    HYP_FORCE_INLINE void StoreStaticString(uint32_t len, const char *str)
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

    HYP_FORCE_INLINE void StoreStaticAddress(bc_address_t addr)
    {
        Value sv;
        sv.m_type = Value::ADDRESS;
        sv.m_value.addr = addr;

        state->m_static_memory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void StoreStaticFunction(bc_address_t addr,
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
        hv->Assign(TypeInfo(type_name, size, names));

        Value sv;
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;
        state->m_static_memory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void LoadI32(bc_reg_t reg, aint32 i32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::I32;
        value.m_value.i32 = i32;
    }

    HYP_FORCE_INLINE void LoadI64(bc_reg_t reg, aint64 i64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::I64;
        value.m_value.i64 = i64;
    }

    HYP_FORCE_INLINE void LoadF32(bc_reg_t reg, afloat32 f32)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::F32;
        value.m_value.f = f32;
    }

    HYP_FORCE_INLINE void LoadF64(bc_reg_t reg, afloat64 f64)
    {
        // get register value given
        Value &value = thread->m_regs[reg];
        value.m_type = Value::F64;
        value.m_value.d = f64;
    }

    HYP_FORCE_INLINE void LoadOffset(bc_reg_t reg, uint16_t offset)
    {
        // read value from stack at (sp - offset)
        // into the the register
        thread->m_regs[reg] = thread->m_stack[thread->m_stack.GetStackPointer() - offset];
    }

    HYP_FORCE_INLINE void LoadIndex(bc_reg_t reg, uint16_t index)
    {
        // read value from stack at the index into the the register
        // NOTE: read from main thread
        thread->m_regs[reg] = state->MAIN_THREAD->m_stack[index];
    }

    HYP_FORCE_INLINE void LoadStatic(bc_reg_t reg, uint16_t index)
    {
        // read value from static memory
        // at the index into the the register
        thread->m_regs[reg] = state->m_static_memory[index];
    }

    HYP_FORCE_INLINE void LoadString(bc_reg_t reg, uint32_t len, const char *str)
    {
        // allocate heap value
        HeapValue *hv = state->HeapAlloc(thread);
        if (hv != nullptr) {
            hv->Assign(ImmutableString(str));

            // assign register value to the allocated object
            Value &sv = thread->m_regs[reg];
            sv.m_type = Value::HEAP_POINTER;
            sv.m_value.ptr = hv;
        }
    }

    HYP_FORCE_INLINE void LoadAddr(bc_reg_t reg, bc_address_t addr)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::ADDRESS;
        sv.m_value.addr = addr;
    }

    HYP_FORCE_INLINE void LoadFunc(bc_reg_t reg,
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

    HYP_FORCE_INLINE void LoadType(bc_reg_t reg,
        uint16_t type_name_len,
        const char *type_name,
        uint16_t size,
        char **names)
    {
        // allocate heap value
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);

        hv->Assign(TypeInfo(type_name, size, names));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;
    }

    HYP_FORCE_INLINE void LoadMem(bc_reg_t dst, bc_reg_t src, uint8_t index)
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
                const vm::TypeInfo *type_ptr = obj_ptr->GetTypePtr();
                
                AssertThrow(type_ptr != nullptr);
                AssertThrow(index < type_ptr->GetSize());
                
                thread->m_regs[dst] = obj_ptr->GetMember(index).value;
                return;
            }
        }

        state->ThrowException(
            thread,
            Exception("Not an Object")
        );
    }

    HYP_FORCE_INLINE void LoadMemHash(bc_reg_t dst_reg, bc_reg_t src_reg, uint32_t hash)
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

    HYP_FORCE_INLINE void LoadArrayIdx(bc_reg_t dst_reg, bc_reg_t src_reg, bc_reg_t index_reg)
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
                Exception("Array index must be integer")
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

    HYP_FORCE_INLINE void LoadRef(bc_reg_t dst_reg, bc_reg_t src_reg)
    {
        Value &src = thread->m_regs[dst_reg];
        src.m_type = Value::VALUE_REF;
        src.m_value.value_ref = &thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void LoadDeref(bc_reg_t dst_reg, bc_reg_t src_reg)
    {
        Value &src = thread->m_regs[src_reg];
        AssertThrowMsg(src.m_type == Value::VALUE_REF, "Value type must be VALUE_REF in order to deref");
        AssertThrow(src.m_value.value_ref != nullptr);

        thread->m_regs[dst_reg] = *src.m_value.value_ref;
    }

    HYP_FORCE_INLINE void LoadNull(bc_reg_t reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = nullptr;
    }

    HYP_FORCE_INLINE void LoadTrue(bc_reg_t reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::BOOLEAN;
        sv.m_value.b = true;
    }

    HYP_FORCE_INLINE void LoadFalse(bc_reg_t reg)
    {
        Value &sv = thread->m_regs[reg];
        sv.m_type = Value::BOOLEAN;
        sv.m_value.b = false;
    }

    HYP_FORCE_INLINE void MovOffset(uint16_t offset, bc_reg_t reg)
    {
        // copy value from register to stack value at (sp - offset)
        thread->m_stack[thread->m_stack.GetStackPointer() - offset] =
            thread->m_regs[reg];
    }

    HYP_FORCE_INLINE void MovIndex(uint16_t index, bc_reg_t reg)
    {
        // copy value from register to stack value at index
        // NOTE: storing on main thread
        state->MAIN_THREAD->m_stack[index] = thread->m_regs[reg];
    }

    HYP_FORCE_INLINE void MovMem(bc_reg_t dst_reg, uint8_t index, bc_reg_t src_reg)
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

        const vm::TypeInfo *type_ptr = object->GetTypePtr();
        AssertThrow(type_ptr != nullptr);

        if (index >= type_ptr->GetSize()) {
            state->ThrowException(
                thread,
                Exception::OutOfBoundsException()
            );
            return;
        }
        
        object->GetMember(index).value = thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void MovMemHash(bc_reg_t dst_reg, uint32_t hash, bc_reg_t src_reg)
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

    HYP_FORCE_INLINE void MovArrayIdx(bc_reg_t dst_reg, uint32_t index, bc_reg_t src_reg)
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

    HYP_FORCE_INLINE void MovReg(bc_reg_t dst_reg, bc_reg_t src_reg)
    {
        thread->m_regs[dst_reg] = thread->m_regs[src_reg];
    }

    HYP_FORCE_INLINE void HasMemHash(bc_reg_t dst_reg, bc_reg_t src_reg, uint32_t hash)
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

    HYP_FORCE_INLINE void Push(bc_reg_t reg)
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

    HYP_FORCE_INLINE void PushArray(bc_reg_t dst_reg, bc_reg_t src_reg)
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

    HYP_FORCE_INLINE void Echo(bc_reg_t reg)
    {
        VM::Print(thread->m_regs[reg]);
    }

    HYP_FORCE_INLINE void EchoNewline()
    {
        utf::fputs(UTF8_CSTR("\n"), stdout);
    }

    HYP_FORCE_INLINE void Jmp(bc_address_t addr)
    {
        bs->Seek(addr);
    }

    HYP_FORCE_INLINE void Je(bc_address_t addr)
    {
        if (thread->m_regs.m_flags == EQUAL) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jne(bc_address_t addr)
    {
        if (thread->m_regs.m_flags != EQUAL) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jg(bc_address_t addr)
    {
        if (thread->m_regs.m_flags == GREATER) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jge(bc_address_t addr)
    {
        if (thread->m_regs.m_flags == GREATER || thread->m_regs.m_flags == EQUAL) {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Call(bc_reg_t reg, uint8_t nargs)
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
        bs->Seek(top.GetValue().call.addr);

        // increase stack size by the amount required by the call
        thread->GetStack().m_sp += top.GetValue().call.varargs_push - 1;
        // NOTE: the -1 is because we will be popping the FUNCTION_CALL 
        // object from the stack anyway...

        // decrease function depth
        thread->m_func_depth--;
    }

    HYP_FORCE_INLINE void BeginTry(bc_address_t addr)
    {
        thread->m_exception_state.m_try_counter++;

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
        AssertThrow(thread->m_exception_state.m_try_counter > 0);

        // pop try catch info
        thread->m_stack.Pop();
        thread->m_exception_state.m_try_counter--;
    }

    HYP_FORCE_INLINE void New(bc_reg_t dst, bc_reg_t src)
    {
        // read value from register
        Value &type_sv = thread->m_regs[src];
        AssertThrow(type_sv.m_type == Value::HEAP_POINTER);

        TypeInfo *type_ptr = type_sv.m_value.ptr->GetPointer<TypeInfo>();
        AssertThrow(type_ptr != nullptr);

        // allocate heap object
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);
        
        // create the Object from the info type_ptr provides us with.
        hv->Assign(Object(type_ptr, type_sv));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;
    }

    HYP_FORCE_INLINE void NewProto(bc_reg_t dst, bc_reg_t src)
    {
        // read value from register
        Value &proto = thread->m_regs[src];
        AssertThrow(proto.m_type == Value::HEAP_POINTER);

        Object *proto_ptr = proto.m_value.ptr->GetPointer<Object>();
        AssertThrow(proto_ptr != nullptr);

        // allocate heap object
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);
        
        // create the Object as a copy of the prototype
        hv->Assign(Object(*proto_ptr));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;
    }

    HYP_FORCE_INLINE void NewArray(bc_reg_t dst, uint32_t size)
    {
        // allocate heap object
        HeapValue *hv = state->HeapAlloc(thread);
        AssertThrow(hv != nullptr);

        hv->Assign(Array(size));

        // assign register value to the allocated object
        Value &sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.ptr = hv;
    }

    HYP_FORCE_INLINE void Cmp(bc_reg_t lhs_reg, bc_reg_t rhs_reg)
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
            afloat64 f;
        } a, b;

        // compare integers
        if (lhs->GetInteger(&a.i) && rhs->GetInteger(&b.i)) {
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

    HYP_FORCE_INLINE void CmpZ(bc_reg_t reg)
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

    HYP_FORCE_INLINE void Add(
        bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        union {
            aint64 i;
            afloat64 f;
        } a, b;

        if (lhs->GetInteger(&a.i) && rhs->GetInteger(&b.i)) {
            aint64 result_value = a.i + b.i;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
        } else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f)) {
            afloat64 result_value = a.f + b.f;
            if (result.m_type == Value::F32) {
                result.m_value.f = result_value;
            } else {
                result.m_value.d = result_value;
            }
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

    HYP_FORCE_INLINE void Sub(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        union {
            aint64 i;
            afloat64 f;
        } a, b;

        if (lhs->GetInteger(&a.i) && rhs->GetInteger(&b.i)) {
            aint64 result_value = a.i - b.i;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
        } else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f)) {
            afloat64 result_value = a.f - b.f;
            if (result.m_type == Value::F32) {
                result.m_value.f = result_value;
            } else {
                result.m_value.d = result_value;
            }
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

    HYP_FORCE_INLINE void Mul(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        union {
            aint64 i;
            afloat64 f;
        } a, b;

        if (lhs->GetInteger(&a.i) && rhs->GetInteger(&b.i)) {
            aint64 result_value = a.i * b.i;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
        } else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f)) {
            afloat64 result_value = a.f * b.f;
            if (result.m_type == Value::F32) {
                result.m_value.f = result_value;
            } else {
                result.m_value.d = result_value;
            }
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

    HYP_FORCE_INLINE void Div(bc_reg_t lhs_reg, bc_reg_t rhs_reg, bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        union {
            aint64 i;
            afloat64 f;
        } a, b;

        if (lhs->GetInteger(&a.i) && rhs->GetInteger(&b.i)) {
            if (b.i == 0) {
                // division by zero
                state->ThrowException(thread, Exception::DivisionByZeroException());
            } else {
                aint64 result_value = a.i / b.i;
                if (result.m_type == Value::I32) {
                    result.m_value.i32 = result_value;
                } else {
                    result.m_value.i64 = result_value;
                }
            }
        } else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f)) {
            if (b.f == 0.0) {
                // division by zero
                state->ThrowException(thread, Exception::DivisionByZeroException());
            } else {
                afloat64 result_value = a.f / b.f;
                if (result.m_type == Value::F32) {
                    result.m_value.f = result_value;
                } else {
                    result.m_value.d = result_value;
                }
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

    HYP_FORCE_INLINE void Mod(bc_reg_t lhs_reg, bc_reg_t rhs_reg, bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        union {
            aint64 i;
            afloat64 f;
        } a, b;

        if (lhs->GetInteger(&a.i) && rhs->GetInteger(&b.i)) {
            if (b.i == 0) {
                // division by zero
                state->ThrowException(
                    thread,
                    Exception::DivisionByZeroException()
                );
            } else {
                aint64 result_value = a.i % b.i;
                if (result.m_type == Value::I32) {
                    result.m_value.i32 = result_value;
                } else {
                    result.m_value.i64 = result_value;
                }
            }
        } else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f)) {
            if (b.f == 0.0) {
                // division by zero
                state->ThrowException(
                    thread,
                    Exception::DivisionByZeroException()
                );
            } else {
                afloat64 result_value = std::fmod(a.f, b.f);
                if (result.m_type == Value::F32) {
                    result.m_value.f = result_value;
                } else {
                    result.m_value.d = result_value;
                }
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

    HYP_FORCE_INLINE void And(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        aint64 a, b;

        if (lhs->GetInteger(&a) && rhs->GetInteger(&b)) {
            aint64 result_value = a & b;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
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

    HYP_FORCE_INLINE void Or(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        aint64 a, b;

        if (lhs->GetInteger(&a) && rhs->GetInteger(&b)) {
            aint64 result_value = a | b;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
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

    HYP_FORCE_INLINE void Xor(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        aint64 a, b;

        if (lhs->GetInteger(&a) && rhs->GetInteger(&b)) {
            aint64 result_value = a ^ b;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
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

    HYP_FORCE_INLINE void Shl(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        aint64 a, b;

        if (lhs->GetInteger(&a) && rhs->GetInteger(&b)) {
            aint64 result_value = a << b;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
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

    HYP_FORCE_INLINE void Shr(bc_reg_t lhs_reg,
        bc_reg_t rhs_reg,
        bc_reg_t dst_reg)
    {
        // load values from registers
        Value *lhs = &thread->m_regs[lhs_reg];
        Value *rhs = &thread->m_regs[rhs_reg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        aint64 a, b;

        if (lhs->GetInteger(&a) && rhs->GetInteger(&b)) {
            aint64 result_value = a >> b;
            if (result.m_type == Value::I32) {
                result.m_value.i32 = result_value;
            } else {
                result.m_value.i64 = result_value;
            }
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

    HYP_FORCE_INLINE void Neg(bc_reg_t reg)
    {
        // load value from register
        Value *value = &thread->m_regs[reg];

        union {
            aint64 i;
            afloat64 f;
        };

        if (value->GetInteger(&i)) {
            if (value->m_type == Value::I32) {
                value->m_value.i32 = -i;
            } else {
                value->m_value.i64 = -i;
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
};

} // namespace vm
} // namespace hyperion

#endif
