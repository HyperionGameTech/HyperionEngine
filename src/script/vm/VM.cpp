#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMTypeInfo.hpp>
#include <script/vm/InstructionHandler.hpp>

#include <Types.hpp>
#include <script/Instructions.hpp>
#include <script/Hasher.hpp>
#include <system/Debug.hpp>

#include <algorithm>
#include <cstdio>
#include <cinttypes>
#include <mutex>
#include <sstream>

namespace hyperion {
namespace vm {

HYP_FORCE_INLINE static void HandleInstruction(
    InstructionHandler &handler,
    BytecodeStream *bs,
    UByte code
)
{

    switch (code) {
    case STORE_STATIC_STRING: {
        // get string length
        UInt32 len; bs->Read(&len);

        // read string based on length
        char *str = new char[len + 1];
        bs->Read(str, len);
        str[len] = '\0';

        handler.StoreStaticString(
            len,
            str
        );

        delete[] str;

        break;
    }
    case STORE_STATIC_ADDRESS: {
        BCAddress addr; bs->Read(&addr);

        handler.StoreStaticAddress(
            addr
        );

        break;
    }
    case STORE_STATIC_FUNCTION: {
        BCAddress addr; bs->Read(&addr);
        UInt8 nargs; bs->Read(&nargs);
        UInt8 flags; bs->Read(&flags);

        handler.StoreStaticFunction(
            addr,
            nargs,
            flags
        );

        break;
    }
    case STORE_STATIC_TYPE: {
        UInt16 type_name_len; bs->Read(&type_name_len);

        char *type_name = new char[type_name_len + 1];
        type_name[type_name_len] = '\0';
        bs->Read(type_name, type_name_len);

        UInt16 size; bs->Read(&size);

        AssertThrow(size > 0);

        char **names = new char*[size];
        // load each name
        for (int i = 0; i < size; i++) {
            UInt16 length;
            bs->Read(&length);

            names[i] = new char[length + 1];
            names[i][length] = '\0';
            bs->Read(names[i], length);
        }

        handler.StoreStaticType(
            type_name,
            size,
            names
        );

        delete[] type_name;
        
        // delete the names
        for (UInt16 i = 0; i < size; i++) {
            delete[] names[i];
        }

        delete[] names;

        break;
    }
    case LOAD_I32: {
        BCRegister reg; bs->Read(&reg);
        int32_t i32; bs->Read(&i32);

        handler.LoadI32(
            reg,
            i32
        );

        break;
    }
    case LOAD_I64: {
        BCRegister reg; bs->Read(&reg);
        int64_t i64; bs->Read(&i64);

        handler.LoadI64(
            reg,
            i64
        );

        break;
    }
    case LOAD_U32: {
        BCRegister reg; bs->Read(&reg);
        UInt32 u32; bs->Read(&u32);

        handler.LoadU32(
            reg,
            u32
        );

        break;
    }
    case LOAD_U64: {
        BCRegister reg; bs->Read(&reg);
        uint64_t u64; bs->Read(&u64);

        handler.LoadU64(
            reg,
            u64
        );

        break;
    }
    case LOAD_F32: {
        BCRegister reg; bs->Read(&reg);
        float f32; bs->Read(&f32);

        handler.LoadF32(
            reg,
            f32
        );

        break;
    }
    case LOAD_F64: {
        BCRegister reg; bs->Read(&reg);
        double f64; bs->Read(&f64);

        handler.LoadF64(
            reg,
            f64
        );

        break;
    }
    case LOAD_OFFSET: {
        BCRegister reg; bs->Read(&reg);
        UInt16 offset; bs->Read(&offset);

        handler.LoadOffset(
            reg,
            offset
        );

        break;
    }
    case LOAD_INDEX: {
        BCRegister reg; bs->Read(&reg);
        UInt16 index; bs->Read(&index);

        handler.LoadIndex(
            reg,
            index
        );

        break;
    }
    case LOAD_STATIC: {
        BCRegister reg; bs->Read(&reg);
        UInt16 index; bs->Read(&index);

        handler.LoadStatic(
            reg,
            index
        );

        break;
    }
    case LOAD_STRING: {
        BCRegister reg; bs->Read(&reg);
        // get string length
        UInt32 len; bs->Read(&len);

        // read string based on length
        char *str = new char[len + 1];
        str[len] = '\0';
        bs->Read(str, len);

        handler.LoadConstantString(
            reg,
            len,
            str
        );

        delete[] str;

        break;
    }
    case LOAD_ADDR: {
        BCRegister reg; bs->Read(&reg);
        BCAddress addr; bs->Read(&addr);

        handler.LoadAddr(
            reg,
            addr
        );

        break;
    }
    case LOAD_FUNC: {
        BCRegister reg; bs->Read(&reg);
        BCAddress addr; bs->Read(&addr);
        UInt8 nargs; bs->Read(&nargs);
        UInt8 flags; bs->Read(&flags);

        handler.LoadFunc(
            reg,
            addr,
            nargs,
            flags
        );

        break;
    }
    case LOAD_TYPE: {
        BCRegister reg; bs->Read(&reg);
        UInt16 type_name_len; bs->Read(&type_name_len);

        char *type_name = new char[type_name_len + 1];
        type_name[type_name_len] = '\0';
        bs->Read(type_name, type_name_len);

        // number of members
        UInt16 size;
        bs->Read(&size);

        char **names = new char*[size];
        
        // load each name
        for (UInt16 i = 0; i < size; i++) {
            UInt16 length;
            bs->Read(&length);

            names[i] = new char[length + 1];
            names[i][length] = '\0';
            bs->Read(names[i], length);
        }

        handler.LoadType(
            reg,
            type_name_len,
            type_name,
            size,
            names
        );

        delete[] type_name;
        
        // delete the names
        for (size_t i = 0; i < size; i++) {
            delete[] names[i];
        }

        delete[] names;

        break;
    }
    case LOAD_MEM: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);
        UInt8 index; bs->Read(&index);

        handler.LoadMem(
            dst,
            src,
            index
        );

        break;
    }
    case LOAD_MEM_HASH: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);
        UInt32 hash; bs->Read(&hash);

        handler.LoadMemHash(
            dst,
            src,
            hash
        );

        break;
    }
    case LOAD_ARRAYIDX: {
        BCRegister dst_reg; bs->Read(&dst_reg);
        BCRegister src_reg; bs->Read(&src_reg);
        BCRegister index_reg; bs->Read(&index_reg);

        handler.LoadArrayIdx(
            dst_reg,
            src_reg,
            index_reg
        );

        break;
    }
    case LOAD_OFFSET_REF: {
        BCRegister reg; bs->Read(&reg);
        UInt16 offset; bs->Read(&offset);

        handler.LoadOffsetRef(
            reg,
            offset
        );

        break;
    }
    case LOAD_INDEX_REF: {
        BCRegister reg; bs->Read(&reg);
        UInt16 index; bs->Read(&index);

        handler.LoadIndexRef(
            reg,
            index
        );

        break;
    }
    case REF: {
        BCRegister dst_reg;
        BCRegister src_reg;

        bs->Read(&dst_reg);
        bs->Read(&src_reg);

        handler.LoadRef(
            dst_reg,
            src_reg
        );

        break;
    }
    case DEREF: {
        BCRegister dst_reg;
        BCRegister src_reg;

        bs->Read(&dst_reg);
        bs->Read(&src_reg);

        handler.LoadDeref(
            dst_reg,
            src_reg
        );

        break;
    }
    case LOAD_NULL: {
        BCRegister reg; bs->Read(&reg);

        handler.LoadNull(
            reg
        );

        break;
    }
    case LOAD_TRUE: {
        BCRegister reg; bs->Read(&reg);

        handler.LoadTrue(
            reg
        );

        break;
    }
    case LOAD_FALSE: {
        BCRegister reg; bs->Read(&reg);

        handler.LoadFalse(
            reg
        );

        break;
    }
    case MOV_OFFSET: {
        UInt16 offset; bs->Read(&offset);
        BCRegister reg; bs->Read(&reg);

        handler.MovOffset(
            offset,
            reg
        );

        break;
    }
    case MOV_INDEX: {
        UInt16 index; bs->Read(&index);
        BCRegister reg; bs->Read(&reg);

        handler.MovIndex(
            index,
            reg
        );

        break;
    }
    case MOV_STATIC: {
        UInt16 index; bs->Read(&index);
        BCRegister reg; bs->Read(&reg);

        handler.MovStatic(
            index,
            reg
        );

        break;
    }
    case MOV_MEM: {
        BCRegister dst; bs->Read(&dst);
        UInt8 index; bs->Read(&index);
        BCRegister src; bs->Read(&src);

        handler.MovMem(
            dst,
            index,
            src
        );

        break;
    }
    case MOV_MEM_HASH: {
        BCRegister dst; bs->Read(&dst);
        UInt32 hash; bs->Read(&hash);
        BCRegister src; bs->Read(&src);

        handler.MovMemHash(
            dst,
            hash,
            src
        );

        break;
    }
    case MOV_ARRAYIDX: {
        BCRegister dst; bs->Read(&dst);
        UInt32 index; bs->Read(&index); // should be int64?
        BCRegister src; bs->Read(&src);

        handler.MovArrayIdx(
            dst,
            index,
            src
        );

        break;
    }
    case MOV_ARRAYIDX_REG: {
        BCRegister dst; bs->Read(&dst);
        BCRegister index_reg; bs->Read(&index_reg);
        BCRegister src; bs->Read(&src);

        handler.MovArrayIdxReg(
            dst,
            index_reg,
            src
        );

        break;
    }
    case MOV_REG: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);
        
        handler.MovReg(
            dst,
            src
        );

        break;
    }
    case HAS_MEM_HASH: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);
        UInt32 hash; bs->Read(&hash);

        handler.HasMemHash(
            dst,
            src,
            hash
        );

        break;
    }
    case PUSH: {
        BCRegister reg; bs->Read(&reg);

        handler.Push(
            reg
        );

        break;
    }
    case POP: {
        handler.Pop();

        break;
    }
    case PUSH_ARRAY: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.PushArray(
            dst,
            src
        );

        break;
    }
    case ADD_SP: {
        UInt16 val; bs->Read(&val);

        handler.AddSp(
            val
        );

        break;
    }
    case SUB_SP: {
        UInt16 val; bs->Read(&val);

        handler.SubSp(
            val
        );

        break;
    }
    case JMP: {
        //BCRegister reg; bs->Read(&reg);
        BCAddress addr;
        bs->Read(&addr);

        handler.Jmp(
            addr
        );

        break;
    }
    case JE: {
        BCAddress addr;
        bs->Read(&addr);

        handler.Je(
            addr
        );

        break;
    }
    case JNE: {
        BCAddress addr;
        bs->Read(&addr);

        handler.Jne(
            addr
        );

        break;
    }
    case JG: {
        BCAddress addr;
        bs->Read(&addr);

        handler.Jg(
            addr
        );

        break;
    }
    case JGE: {
        BCAddress addr;
        bs->Read(&addr);

        handler.Jge(
            addr
        );

        break;
    }
    case CALL: {
        BCRegister reg; bs->Read(&reg);
        UInt8 nargs; bs->Read(&nargs);

        handler.Call(
            reg,
            nargs
        );

        break;
    }
    case RET: {
        handler.Ret();
        
        break;
    }
    case BEGIN_TRY: {
        BCAddress catch_address;
        bs->Read(&catch_address);

        handler.BeginTry(
            catch_address
        );

        break;
    }
    case END_TRY: {
        handler.EndTry();

        break;
    }
    case NEW: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.New(
            dst,
            src
        );

        break;
    }
    case NEW_ARRAY: {
        BCRegister dst; bs->Read(&dst);
        UInt32 size; bs->Read(&size);

        handler.NewArray(
            dst,
            size
        );

        break;
    }
    case CMP: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);

        handler.Cmp(
            lhs_reg,
            rhs_reg
        );

        break;
    }
    case CMPZ: {
        BCRegister reg; bs->Read(&reg);

        handler.CmpZ(
            reg
        );

        break;
    }
    case ADD: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Add(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case SUB: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Sub(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case MUL: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Mul(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case DIV: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Div(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case MOD: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Mod(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case AND: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.And(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case OR: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Or(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case XOR: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Xor(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case SHL: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Shl(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case SHR: {
        BCRegister lhs_reg; bs->Read(&lhs_reg);
        BCRegister rhs_reg; bs->Read(&rhs_reg);
        BCRegister dst_reg; bs->Read(&dst_reg);

        handler.Shr(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case NEG: {
        BCRegister reg; bs->Read(&reg);

        handler.Neg(
            reg
        );

        break;
    }
    case NOT: {
        BCRegister reg; bs->Read(&reg);

        handler.Not(
            reg
        );

        break;
    }
    case THROW: {
        BCRegister reg; bs->Read(&reg);

        handler.Throw(
            reg
        );

        break;
    }
    case TRACEMAP: {
        UInt32 len; bs->Read(&len);

        UInt32 stringmap_count;
        bs->Read(&stringmap_count);

        Tracemap::StringmapEntry *stringmap = nullptr;

        if (stringmap_count != 0) {
            stringmap = new Tracemap::StringmapEntry[stringmap_count];

            for (UInt32 i = 0; i < stringmap_count; i++) {
                bs->Read(&stringmap[i].entry_type);
                bs->ReadZeroTerminatedString(stringmap[i].data);
            }
        }

        UInt32 linemap_count;
        bs->Read(&linemap_count);

        Tracemap::LinemapEntry *linemap = nullptr;

        if (linemap_count != 0) {
            linemap = new Tracemap::LinemapEntry[linemap_count];
            bs->Read(linemap, sizeof(Tracemap::LinemapEntry) * linemap_count);
        }

        handler.state->m_tracemap.Set(stringmap, linemap);

        break;
    }
    case REM: {
        UInt32 len; bs->Read(&len);
        // just skip comment
        bs->Skip(len);

        break;
    }
    case EXPORT: {
        BCRegister reg; bs->Read(&reg);
        UInt32 hash; bs->Read(&hash);

        handler.ExportSymbol(
            reg,
            hash
        );

        break;
    }
    case CAST_U8: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastU8(
            dst,
            src
        );

        break;
    }
    case CAST_U16: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastU16(
            dst,
            src
        );

        break;
    }
    case CAST_U32: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastU32(
            dst,
            src
        );

        break;
    }
    case CAST_U64: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastU64(
            dst,
            src
        );

        break;
    }
    case CAST_I8: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastI8(
            dst,
            src
        );

        break;
    }
    case CAST_I16: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastI16(
            dst,
            src
        );

        break;
    }
    case CAST_I32: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastI32(
            dst,
            src
        );

        break;
    }
    case CAST_I64: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastI64(
            dst,
            src
        );

        break;
    }
    case CAST_F32: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastF32(
            dst,
            src
        );

        break;
    }
    case CAST_F64: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastF64(
            dst,
            src
        );

        break;
    }
    case CAST_BOOL: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastBool(
            dst,
            src
        );

        break;
    }
    case CAST_DYNAMIC: {
        BCRegister dst; bs->Read(&dst);
        BCRegister src; bs->Read(&src);

        handler.CastDynamic(
            dst,
            src
        );

        break;
    }
    default: {
        Int64 last_pos = Int64(bs->Position()) - sizeof(UByte);
        utf::printf(HYP_UTF8_CSTR("unknown instruction '%d' referenced at location: 0x%" PRIx64 "\n"), code, last_pos);
        // seek to end of bytecode stream
        bs->Seek(bs->Size());

        return;
    }
    }
}

VM::VM(APIInstance &api_instance)
    : m_api_instance(api_instance)
{
    m_state.m_vm = non_owning_ptr<VM>(this);
    // create main thread
    m_state.CreateThread();
}

VM::~VM()
{
}

void VM::PushNativeFunctionPtr(NativeFunctionPtr_t ptr)
{
    Value sv;
    sv.m_type = Value::NATIVE_FUNCTION;
    sv.m_value.native_func = ptr;

    AssertThrow(m_state.GetMainThread() != nullptr);
    m_state.GetMainThread()->m_stack.Push(sv);
}

void VM::Invoke(
    InstructionHandler *handler,
    const Value &value,
    UInt8 nargs
)
{
    static const UInt32 invoke_hash = hash_fnv_1("$invoke");

    VMState *state = handler->state;
    ExecutionThread *thread = handler->thread;
    BytecodeStream *bs = handler->bs;

    AssertThrow(state != nullptr);
    AssertThrow(thread != nullptr);
    AssertThrow(bs != nullptr);

    if (value.m_type != Value::FUNCTION) {
        if (value.m_type == Value::NATIVE_FUNCTION) {
            Value **args = new Value*[nargs > 0 ? nargs : 1];

            Int64 i = static_cast<Int64>(thread->m_stack.GetStackPointer()) - 1;
            for (int j = nargs - 1; j >= 0 && i >= 0; i--, j--) {
                args[j] = &thread->m_stack[i];
            }

            sdk::Params params {
                .api_instance   = m_api_instance,
                .handler        = handler,
                .args           = args,
                .nargs          = nargs
            };

            // disable auto gc so no collections happen during a native function
            state->enable_auto_gc = false;

            // call the native function
            value.m_value.native_func(params);

            // re-enable auto gc
            state->enable_auto_gc = ENABLE_GC;

            delete[] args;

            return;
        } else if (value.m_type == Value::HEAP_POINTER) {
            if (value.m_value.ptr == nullptr) {
                state->ThrowException(
                    thread,
                    Exception::NullReferenceException()
                );
                return;
            } else if (VMObject *object = value.m_value.ptr->GetPointer<VMObject>()) {
                if (Member *member = object->LookupMemberFromHash(invoke_hash)) {
                    const Int64 sp = static_cast<Int64>(thread->m_stack.GetStackPointer());
                    const Int64 args_start = sp - nargs;

                    if (nargs > 0) {
                        // shift over by 1 -- and insert 'self' to start of args
                        // make a copy of last item to not overwrite it
                        thread->m_stack.Push(thread->m_stack[sp - 1]);

                        for (SizeType i = args_start; i < sp - 1; i++) {
                            thread->m_stack[i + 1] = thread->m_stack[i];
                        }
                        
                        // set 'self' object to start of args
                        thread->m_stack[args_start] = value;
                    } else {
                        thread->m_stack.Push(value);
                    }

                    Invoke(
                        handler,
                        member->value,
                        nargs + 1
                    );

                    Value &top = thread->m_stack.Top();
                    AssertThrow(top.m_type == Value::FUNCTION_CALL);

                    // bookkeeping to remove the closure object
                    // normally, arguments are popped after the call is returned,
                    // rather than within the body
                    top.m_value.call.varargs_push--;

                    return;
                }
            }
        }

        char buffer[255];
        std::sprintf(
            buffer,
            "cannot invoke type '%s' as a function",
            value.GetTypeString()
        );

        state->ThrowException(
            thread,
            Exception(buffer)
        );

        return;
    }
    
    if ((value.m_value.func.m_flags & FunctionFlags::VARIADIC) && nargs < value.m_value.func.m_nargs - 1) {
        // if variadic, make sure the arg count is /at least/ what is required
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                value.m_value.func.m_nargs,
                nargs,
                true
            )
        );
    } else if (!(value.m_value.func.m_flags & FunctionFlags::VARIADIC) && value.m_value.func.m_nargs != nargs) {
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                value.m_value.func.m_nargs,
                nargs
            )
        );
    } else {
        Value previous_addr;
        previous_addr.m_type = Value::FUNCTION_CALL;
        previous_addr.m_value.call.varargs_push = 0;
        previous_addr.m_value.call.return_address = static_cast<BCAddress>(bs->Position());

        if (value.m_value.func.m_flags & FunctionFlags::VARIADIC) {
            // for each argument that is over the expected size, we must pop it from
            // the stack and add it to a new array.
            int varargs_amt = nargs - value.m_value.func.m_nargs + 1;
            if (varargs_amt < 0) {
                varargs_amt = 0;
            }
            
            // set varargs_push value so we know how to get back to the stack size before.
            previous_addr.m_value.call.varargs_push = varargs_amt - 1;
            
            // allocate heap object
            HeapValue *hv = state->HeapAlloc(thread);
            AssertThrow(hv != nullptr);

            // create VMArray object to hold variadic args
            VMArray arr(varargs_amt);

            for (int i = varargs_amt - 1; i >= 0; i--) {
                // push to array
                arr.AtIndex(i, thread->GetStack().Top());
                thread->GetStack().Pop();
            }

            // assign heap value to our array
            hv->Assign(std::move(arr));

            Value array_value;
            array_value.m_type = Value::HEAP_POINTER;
            array_value.m_value.ptr = hv;

            hv->Mark();

            // push the array to the stack
            thread->GetStack().Push(array_value);
        }

        // push the address
        thread->GetStack().Push(previous_addr);
        // seek to the new address
        bs->Seek(value.m_value.func.m_addr);

        // increase function depth
        thread->m_func_depth++;
    }
}

void VM::InvokeNow(
    BytecodeStream *bs,
    const Value &value,
    UInt8 nargs
)
{
    ExecutionThread *thread = m_state.MAIN_THREAD;
    
    const SizeType position_before = bs->Position();
    const UInt original_function_depth = thread->m_func_depth;
    const SizeType stack_size_before = thread->GetStack().GetStackPointer();

    InstructionHandler handler(
        &m_state,
        thread,
        bs
    );

    Invoke(
        &handler,
        value,
        nargs
    );

    if (value.m_type == Value::FUNCTION) { // don't do this for native function calls
        UByte code;

        while (!bs->Eof()) {
            bs->Read(&code);
            
            HandleInstruction(
                handler,
                bs,
                code
            );

            if (handler.thread->GetExceptionState().HasExceptionOccurred()) {
                if (!HandleException(&handler)) {
                    thread->m_exception_state.m_exception_depth = 0;

                    AssertThrow(thread->GetStack().GetStackPointer() >= stack_size_before);
                    thread->GetStack().Pop(thread->GetStack().GetStackPointer() - stack_size_before);

                    break;
                }
            }

            if (code == RET) {
                if (thread->m_func_depth == original_function_depth) {
                    break;
                }
            }
        }
    }
    
    bs->SetPosition(position_before);
}

void VM::CreateStackTrace(ExecutionThread *thread, StackTrace *out)
{
    const SizeType max_stack_trace_size = std::size(out->call_addresses);

    for (Int &call_address : out->call_addresses) {
        call_address = -1;
    }

    SizeType num_recorded_call_addresses = 0;

    for (SizeType sp = thread->m_stack.GetStackPointer(); sp != 0; sp--) {
        if (num_recorded_call_addresses >= max_stack_trace_size) {
            break;
        }

        const Value &top = thread->m_stack[sp - 1];

        if (top.m_type == Value::FUNCTION_CALL) {
            out->call_addresses[num_recorded_call_addresses++] = static_cast<Int>(top.m_value.call.return_address);
        }
    }
}

bool VM::HandleException(InstructionHandler *handler)
{
    ExecutionThread *thread = handler->thread;
    BytecodeStream *bs = handler->bs;

    if (thread->m_exception_state.m_try_counter != 0) {
        // handle exception
        --thread->m_exception_state.m_try_counter;

        AssertThrow(thread->m_exception_state.m_exception_depth != 0);
        --thread->m_exception_state.m_exception_depth;

        Value *top = nullptr;
        while ((top = &thread->m_stack.Top()) && top->m_type != Value::TRY_CATCH_INFO) {
            thread->m_stack.Pop();
        }

        // top should be exception data
        AssertThrow(top != nullptr && top->m_type == Value::TRY_CATCH_INFO);

        // jump to the catch block
        bs->Seek(top->m_value.try_catch_info.catch_address);

        // pop exception data from stack
        thread->m_stack.Pop();

        return true;
    } else {
        StackTrace stack_trace;
        CreateStackTrace(thread, &stack_trace);

        std::cout << "stack_trace = \n";

        for (auto call_address : stack_trace.call_addresses) {
            if (call_address == -1) {
                break;
            }
            
            std::cout << "\t" << std::hex << call_address << "\n";
        }

        std::cout << "=====\n";

        // TODO: Seek outside function, if calling from outside?
        // so we can keep calling
    }

    return false;
}

void VM::Execute(BytecodeStream *bs)
{
    AssertThrow(bs != nullptr);
    AssertThrow(m_state.GetNumThreads() != 0);

    InstructionHandler handler(
        &m_state,
        m_state.MAIN_THREAD,
        bs
    );

    UByte code;

    while (!bs->Eof()) {
        bs->Read(&code);
        
        HandleInstruction(
            handler,
            bs,
            code
        );

        if (handler.thread->GetExceptionState().HasExceptionOccurred()) {
            HandleException(&handler);

            if (!handler.state->good) {
                DebugLog(LogType::Error, "Unhandled exception in VM, stopping execution...\n");

                break;
            }
        }
    }
}

} // namespace vm
} // namespace hyperion
