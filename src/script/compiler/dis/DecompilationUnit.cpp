#include <script/compiler/dis/DecompilationUnit.hpp>
#include <script/Instructions.hpp>
#include <core/lib/DynArray.hpp>
#include <Types.hpp>

#include <sstream>
#include <cstring>
#include <cstdio>

namespace hyperion::compiler {

DecompilationUnit::DecompilationUnit()
{
}

void DecompilationUnit::DecodeNext(
    UInt8 code,
    hyperion::vm::BytecodeStream &bs,
    InstructionStream &is,
    utf::utf8_ostream *os
)
{
    auto opr = BytecodeUtil::Make<RawOperation<>>();
    opr->opcode = code;

    switch (code) {
    case NOP:
    {
        if (os != nullptr) {
            (*os) << "nop" << std::endl;
        }

        break;
    }
    case STORE_STATIC_STRING:
    {
        UInt32 len;
        bs.Read(&len);

        char *str = new char[len + 1];
        str[len] = '\0';
        bs.Read(str, len);

        if (os != nullptr) {
            (*os)
                << "str ["
                    << "u32(" << len << "), "
                    << "\"" << str << "\""
                << "]"
                << std::endl;
        }

        delete[] str;

        break;
    }
    case STORE_STATIC_ADDRESS:
    {
        UInt32 val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os) << "addr [@(" << std::hex << val << std::dec << ")]" << std::endl;
        }

        break;
    }
    case STORE_STATIC_FUNCTION:
    {
        UInt32 addr;
        bs.Read(&addr);

        UInt8 nargs;
        bs.Read(&nargs);

        UInt8 flags;
        bs.Read(&flags);

        if (os != nullptr) {
            (*os) << "function [@(" << std::hex << addr << std::dec << "), "
                    << "u8(" << (int)nargs << ")], "
                    << "u8(" << (int)flags << ")]"
                    << std::endl;
        }

        break;
    }
    case STORE_STATIC_TYPE:
    {
        UInt16 type_name_len;
        bs.Read(&type_name_len);

        Array<UInt8> type_name;
        type_name.Resize(type_name_len + 1);
        type_name[type_name_len] = '\0';
        bs.Read(&type_name[0], type_name_len);

        UInt16 size;
        bs.Read(&size);

        Array<Array<UInt8>> names;
        names.Resize(size);
        
        for (int i = 0; i < size; i++) {
            UInt16 len;
            bs.Read(&len);

            names[i].Resize(len + 1);
            names[i][len] = '\0';
            bs.Read(&names[i][0], len);
        }

        if (os != nullptr) {
            (*os)
                << "type ["
                    << "str(" << type_name.Data() << "), "
                    << "u16(" << (int)size << "), ";

            for (int i = 0; i < size; i++) {
                (*os) << "str(" << names[i].Data() << ")";
                if (i != size - 1) {
                    (*os) << ", ";
                }
            }
                    
            (*os)
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_I32:
    {
        UInt8 reg;
        bs.Read(&reg);

        int32_t val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "load_i32 ["
                    << "%" << (int)reg << ", "
                    << "i32(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_I64:
    {
        UInt8 reg;
        bs.Read(&reg);

        int64_t val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "load_i64 ["
                    << "%" << (int)reg << ", "
                    << "i64(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_U32:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt32 val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "load_u32 ["
                    << "%" << (int)reg << ", "
                    << "u32(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_U64:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt64 val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "load_u64 ["
                    << "%" << (int)reg << ", "
                    << "u64(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_F32:
    {
        UInt8 reg;
        bs.Read(&reg);

        float val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "load_f32 ["
                    << "%" << (int)reg << ", "
                    << "f32(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_F64:
    {
        UInt8 reg;
        bs.Read(&reg);

        double val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "load_f64 ["
                    << "%" << (int)reg << ", "
                    << "f64(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_OFFSET:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt16 offset;
        bs.Read(&offset);

        if (os != nullptr) {
            (*os)
                << "load_offset ["
                    << "%" << (int)reg << ", "
                    "$(sp-" << offset << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_INDEX:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt16 idx;
        bs.Read(&idx);

        if (os != nullptr) {
            (*os)
                << "load_index ["
                    << "%" << (int)reg << ", "
                    "u16(" << idx << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_STATIC:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt16 index;
        bs.Read(&index);

        if (os != nullptr) {
            (*os)
                << "load_static ["
                    << "%" << (int)reg << ", "
                    << "#" << index
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_STRING:
    {
        UInt8 reg;
        bs.Read(&reg);

        // get string length
        UInt32 len;
        bs.Read(&len);

        // read string based on length
        char *str = new char[len + 1];
        bs.Read(str, len);
        str[len] = '\0';

        if (os != nullptr) {
            (*os)
                << "load_str ["
                    << "%" << (int)reg << ", "
                    << "u32(" << len << "), "
                    << "\"" << str << "\""
                << "]"
                << std::endl;
        }

        delete[] str;

        break;
    }
    case LOAD_ADDR:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt32 val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os) << "load_addr [%" << (int)reg << ", @(" << std::hex << val << std::dec << ")]" << std::endl;
        }

        break;
    }
    case LOAD_FUNC:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt32 addr;
        bs.Read(&addr);

        UInt8 nargs;
        bs.Read(&nargs);

        UInt8 flags;
        bs.Read(&flags);

        if (os != nullptr) {
            (*os) << "load_func [%" << (int)reg
                    << ", @(" << std::hex << addr << std::dec << "), "
                    << "u8(" << (int)nargs << ")], "
                    << "u8(" << (int)flags << ")]"
                    << std::endl;
        }

        break;
    }
    case LOAD_TYPE:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt16 type_name_len;
        bs.Read(&type_name_len);

        Array<UInt8> type_name;
        type_name.Resize(type_name_len + 1);
        type_name[type_name_len] = '\0';
        bs.Read(&type_name[0], type_name_len);

        UInt16 size;
        bs.Read(&size);

        Array<Array<UInt8>> names;
        names.Resize(size);
        
        for (int i = 0; i < size; i++) {
            UInt16 len;
            bs.Read(&len);

            names[i].Resize(len + 1);
            names[i][len] = '\0';
            bs.Read(&names[i][0], len);
        }

        if (os != nullptr) {
            (*os)
                << "load_type ["
                    << "%" << (int)reg << ", "
                    << "str(" << type_name.Data() << "), "
                    << "u16(" << (int)size << ")";

            for (int i = 0; i < size; i++) {
                (*os) << ", str(" << names[i].Data() << ")";
            }
                    
            (*os)
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_MEM:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt8 src;
        bs.Read(&src);

        UInt8 idx;
        bs.Read(&idx);

        if (os != nullptr) {
            (*os)
                << "load_mem ["
                    << "%" << (int)reg << ", "
                    << "%" << (int)src << ", "
                    << "u8(" << (int)idx << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_MEM_HASH:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt8 src;
        bs.Read(&src);

        UInt32 hash;
        bs.Read(&hash);

        if (os != nullptr) {
            (*os)
                << "load_mem_hash ["
                    << "%" << (int)reg << ", "
                    << "%" << (int)src << ", "
                    << "u32(" << hash << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_ARRAYIDX:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt8 src;
        bs.Read(&src);

        UInt8 idx;
        bs.Read(&idx);

        if (os != nullptr) {
            (*os)
                << "load_arrayidx ["
                    << "%" << (int)reg << ", "
                    << "%" << (int)src << ", "
                    << "%" << (int)idx << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_OFFSET_REF:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt16 offset;
        bs.Read(&offset);

        if (os != nullptr) {
            (*os)
                << "load_offset_ref ["
                    << "%" << (int)reg << ", "
                    "$(sp-" << offset << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_INDEX_REF:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt16 idx;
        bs.Read(&idx);

        if (os != nullptr) {
            (*os)
                << "load_index_ref ["
                    << "%" << (int)reg << ", "
                    "u16(" << idx << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case REF:
    {
        UInt8 src_reg;
        UInt8 dst_reg;

        bs.Read(&src_reg);
        bs.Read(&dst_reg);

        if (os != nullptr) {
            (*os)
                << "ref ["
                    << "%" << (int)dst_reg << ", "
                    << "%" << (int)src_reg
                << "]"
                << std::endl;
        }

        break;
    }
    case DEREF:
    {
        UInt8 src_reg;
        UInt8 dst_reg;

        bs.Read(&src_reg);
        bs.Read(&dst_reg);

        if (os != nullptr) {
            (*os)
                << "deref ["
                    << "%" << (int)dst_reg << ", "
                    << "%" << (int)src_reg
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_NULL:
    {
        UInt8 reg;
        bs.Read(&reg);

        if (os != nullptr) {
            (*os)
                << "load_null ["
                    << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_TRUE:
    {
        UInt8 reg;
        bs.Read(&reg);

        if (os != nullptr) {
            (*os)
                << "load_true ["
                    << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_FALSE:
    {
        UInt8 reg;
        bs.Read(&reg);

        if (os != nullptr) {
            (*os)
                << "load_false ["
                    << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_OFFSET:
    {
        UInt16 dst;
        bs.Read(&dst);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_offset ["
                    << "$(sp-" << dst << "), "
                    << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_INDEX:
    {
        UInt16 dst;
        bs.Read(&dst);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_index ["
                    << "u16(" << dst << "), "
                    << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_STATIC:
    {
        UInt16 dst;
        bs.Read(&dst);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_static ["
                    << "#" << dst << ", "
                    << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_MEM:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt8 idx;
        bs.Read(&idx);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_mem ["
                    << "%" << (int)reg << ", "
                    << "u8(" << (int)idx << "), "
                    << "%" << (int)src << ""
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_MEM_HASH:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt32 hash;
        bs.Read(&hash);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_mem_hash ["
                    << "%" << (int)reg << ", "
                    << "u32(" << hash << "), "
                    << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_ARRAYIDX:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt32 idx;
        bs.Read(&idx);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_arrayidx ["
                    << "%" << (int)reg << ", "
                    << "u32(" << (int)idx << "), "
                    << "%" << (int)src << ""
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_ARRAYIDX_REG:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt8 idx;
        bs.Read(&idx);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_arrayidx_reg ["
                    << "%" << (int)reg << ", "
                    << "%" << (int)idx << ", "
                    << "%" << (int)src << ""
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_REG:
    {
        UInt8 dst;
        bs.Read(&dst);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "mov_reg ["
                    << "%" << (int)dst << ", "
                    << "%" << (int)src << ""
                << "]"
                << std::endl;
        }

        break;
    }
    case HAS_MEM_HASH:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt8 src;
        bs.Read(&src);

        UInt32 hash;
        bs.Read(&hash);

        if (os != nullptr) {
            (*os)
                << "has_mem_hash ["
                    << "%" << (int)reg << ", "
                    << "%" << (int)src << ", "
                    << "u32(" << hash << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case PUSH:
    {
        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "push ["
                    << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case POP:
    {
        if (os != nullptr) {
            (*os)
                << "pop"
                << std::endl;
        }

        break;
    }
    case PUSH_ARRAY:
    {
        UInt8 dst;
        bs.Read(&dst);

        UInt8 src;
        bs.Read(&src);

        if (os != nullptr) {
            (*os)
                << "push_array ["
                << "% " << (int)dst << ", "
                << "% " << (int)src
                << "]" << std::endl;
        }

        break;
    }
    case ADD_SP:
    {
        UInt16 val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "add_sp ["
                    << "u16(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case SUB_SP:
    {
        UInt16 val;
        bs.Read(&val);

        if (os != nullptr) {
            (*os)
                << "sub_sp ["
                    << "u16(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JMP:
    {
        UInt32 addr;
        bs.Read(&addr);

        if (os != nullptr) {

            (*os)
                << "jmp ["
                    << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JE:
    {
        UInt32 addr;
        bs.Read(&addr);

        if (os != nullptr) {
            (*os)
                << "je ["
                    << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JNE:
    {
        UInt32 addr;
        bs.Read(&addr);

        if (os != nullptr) {
            (*os)
                << "jne ["
                    << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JG:
    {
        UInt32 addr;
        bs.Read(&addr);

        if (os != nullptr) {
            (*os)
                << "jg ["
                    << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JGE:
    {
        UInt32 addr;
        bs.Read(&addr);

        if (os != nullptr) {
            (*os)
                << "jge ["
                    << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case CALL:
    {
        UInt8 func;
        bs.Read(&func);

        UInt8 argc;
        bs.Read(&argc);

        if (os != nullptr) {
            (*os)
                << "call ["
                    << "%" << (int)func << ", "
                    << "u8(" << (int)argc << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case RET:
    {
        if (os != nullptr) {
            (*os)
                << "ret"
                << std::endl;
        }

        break;
    }
    case BEGIN_TRY:
    {
        UInt32 addr;
        bs.Read(&addr);

        if (os != nullptr) {
            (*os)
                << "begin_try ["
                    << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case END_TRY:
    {
        if (os != nullptr) {
            (*os) << "end_try" << std::endl;
        }

        break;
    }
    case NEW:
    {
        UInt8 dst;
        bs.Read(&dst);

        UInt8 type;
        bs.Read(&type);

        if (os != nullptr) {
            (*os)
                << "new ["
                    << "%" << (int)dst << ", "
                    << "%" << (int)type
                << "]"
                << std::endl;
        }

        break;
    }
    case NEW_ARRAY:
    {
        UInt8 dst;
        bs.Read(&dst);

        UInt32 size;
        bs.Read(&size);

        if (os != nullptr) {
            (*os)
                << "new_array ["
                    << "%"    << (int)dst << ", "
                    << "u32(" << (int)size << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case CMP:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        if (os != nullptr) {
            (*os)
                << "cmp ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs
                << "]"
                << std::endl;
        }

        break;
    }
    case CMPZ:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        if (os != nullptr) {
            (*os)
                << "cmpz ["
                    << "%" << (int)lhs
                << "]"
                << std::endl;
        }

        break;
    }
    case ADD:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "add ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case SUB:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "sub ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case MUL:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "mul ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case DIV:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "div ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case MOD:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "mod ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case AND:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "and ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case OR:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "or ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case XOR:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "xor ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case SHL:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "shl ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }

    case SHR:
    {
        UInt8 lhs;
        bs.Read(&lhs);

        UInt8 rhs;
        bs.Read(&rhs);

        UInt8 dst;
        bs.Read(&dst);

        if (os != nullptr) {
            (*os)
                << "shr ["
                    << "%" << (int)lhs << ", "
                    << "%" << (int)rhs << ", "
                    << "%" << (int)dst
                << "]"
                << std::endl;
        }

        break;
    }
    case NEG:
    {
        UInt8 reg;
        bs.Read(&reg);

        if (os != nullptr) {
            (*os)
                << "neg ["
                    << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case THROW:
    {
        UInt8 reg;
        bs.Read(&reg);

        if (os != nullptr) {
            (*os)
                << "throw ["
                    << "% " << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case TRACEMAP:
    {
        UInt32 len;
        bs.Read(&len);

        if (os != nullptr) {
            (*os)
                << "tracemap ["
                    << "u32(" << len << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case REM:
    {
        UInt32 len;
        bs.Read(&len);

        // read string based on length
        char *str = new char[len + 1];
        bs.Read(str, len);
        str[len] = '\0';

        if (os != nullptr) {
            (*os)
                << "\t; "
                << str
                << std::endl;
        }

        delete[] str;

        break;
    }
    case EXPORT:
    {
        UInt8 reg;
        bs.Read(&reg);

        UInt32 hash;
        bs.Read(&hash);

        if (os != nullptr) {
            (*os)
                << "export ["
                    << "%" << (int)reg << ", "
                    << "u32(" << hash << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U8:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_u8 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U16:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_u16 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U32:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_u32 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U64:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_u64 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I8:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_i8 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I16:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_i16 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I32:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_i32 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I64:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_i64 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_F32:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_f32 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_F64:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_f64 ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_BOOL:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_bool ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_DYNAMIC:
    {
        UInt8 reg_dst;
        bs.Read(&reg_dst);

        UInt8 reg_src;
        bs.Read(&reg_src);

        if (os != nullptr) {
            (*os)
                << "cast_dynamic ["
                    << "%" << (int)reg_dst << ", "
                    << "%" << (int)reg_src
                << "]"
                << std::endl;
        }

        break;
    }
    case EXIT:
    {
        if (os != nullptr) {
            (*os)
                << "exit"
                << std::endl;
        }

        break;
    }
    default:
        if (os != nullptr) {
            (*os)
                << "??"
                << std::endl;
        }
        // unrecognized instruction

        break;
    }
}

InstructionStream DecompilationUnit::Decompile(hyperion::vm::BytecodeStream &bs, utf::utf8_ostream *os)
{
    InstructionStream is;

    while (!bs.Eof()) {
        const size_t pos = bs.Position();
        
        if (os != nullptr) {
            (*os) << std::hex << pos << std::dec << "\t";
        }

        UInt8 code;
        bs.Read(&code);

        DecodeNext(code, bs, is, os);
    }

    return is;
}

} // namespace hyperion::compiler
