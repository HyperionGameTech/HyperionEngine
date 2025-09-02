#include <script/compiler/dis/DecompilationUnit.hpp>
#include <script/Instructions.hpp>
#include <core/containers/Array.hpp>
#include <core/Types.hpp>

#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <sstream>
#include <cstdio>

namespace hyperion::compiler {

DecompilationUnit::DecompilationUnit()
{
}

void DecompilationUnit::DecodeNext(
    uint8 code,
    hyperion::vm::BytecodeStream& bs,
    InstructionStream& is,
    std::ostream* os)
{
    auto opr = BytecodeUtil::Make<RawOperation<>>();
    opr->opcode = code;

    switch (code)
    {
    case LOAD_UNIFIED:
    {
        uint8 subcmd;
        bs.Read(&subcmd);

        uint8 reg;
        bs.Read(&reg);

        const uint8 dataType = GET_LOAD_DTYPE(subcmd);
        const bool isRef = GET_LOAD_ISREF(subcmd);
        const uint8 srcType = GET_LOAD_SRCTYPE(subcmd);

        if (os != nullptr)
        {
            *os << "LOAD ";

            // Print data type
            switch (dataType)
            {
            case DTYPE_I32:
                *os << "i32";
                break;
            case DTYPE_I64:
                *os << "i64";
                break;
            case DTYPE_U32:
                *os << "u32";
                break;
            case DTYPE_U64:
                *os << "u64";
                break;
            case DTYPE_F32:
                *os << "f32";
                break;
            case DTYPE_F64:
                *os << "f64";
                break;
            case DTYPE_BOOL:
                *os << "bool";
                break;
            case DTYPE_OBJECT:
                *os << "obj";
                break;
            }

            if (isRef)
                *os << "_ref";

            *os << " %r" << (int)reg;

            // Print source type and operands
            switch (srcType)
            {
            case LSRC_IMMEDIATE:
                *os << ", ";
                switch (dataType)
                {
                case DTYPE_I32:
                {
                    int32_t value;
                    bs.Read(&value);
                    *os << value;
                }
                break;
                case DTYPE_I64:
                {
                    int64_t value;
                    bs.Read(&value);
                    *os << value;
                }
                break;
                case DTYPE_U32:
                {
                    uint32 value;
                    bs.Read(&value);
                    *os << value;
                }
                break;
                case DTYPE_U64:
                {
                    uint64 value;
                    bs.Read(&value);
                    *os << value;
                }
                break;
                case DTYPE_F32:
                {
                    float value;
                    bs.Read(&value);
                    *os << value;
                }
                break;
                case DTYPE_F64:
                {
                    double value;
                    bs.Read(&value);
                    *os << value;
                }
                break;
                case DTYPE_BOOL:
                {
                    uint8 value;
                    bs.Read(&value);
                    *os << (value ? "true" : "false");
                }
                break;
                case DTYPE_OBJECT:
                    *os << "null";
                    break;
                }
                break;

            case LSRC_OFFSET:
            {
                uint16 offset;
                bs.Read(&offset);
                *os << ", $" << offset;
            }
            break;

            case LSRC_INDEX:
            {
                uint16 index;
                bs.Read(&index);
                *os << ", #" << index;
            }
            break;

            case LSRC_STATIC:
            {
                uint16 index;
                bs.Read(&index);
                *os << ", static[" << index << "]";
            }
            break;

            case LSRC_ARRAYIDX:
            {
                uint8 arrayReg;
                bs.Read(&arrayReg);
                uint8 indexReg;
                bs.Read(&indexReg);
                *os << ", %r" << (int)arrayReg << "[%r" << (int)indexReg << "]";
            }
            break;

            case LSRC_MEMBER:
            {
                uint8 objReg;
                bs.Read(&objReg);
                uint64 hash;
                bs.Read(&hash);
                *os << ", %r" << (int)objReg << ".member@" << std::hex << hash << std::dec;
            }
            break;

            case LSRC_REGISTER:
            {
                uint8 srcReg;
                bs.Read(&srcReg);
                *os << ", %r" << (int)srcReg;
            }
            break;

            case LSRC_ADDRESS:
            {
                uint32 addr;
                bs.Read(&addr);
                *os << ", @" << addr;
            }
            break;
            }

            *os << std::endl;
        }

        break;
    }

    case MOV_UNIFIED:
    {
        uint8 subcmd;
        bs.Read(&subcmd);

        const uint8 dstType = GET_MOV_DSTTYPE(subcmd);
        const uint8 srcType = GET_MOV_SRCTYPE(subcmd);

        if (os != nullptr)
        {
            *os << "MOV ";

            switch (dstType)
            {
            case MDST_OFFSET:
            {
                uint16 offset;
                bs.Read(&offset);
                uint8 srcReg;
                bs.Read(&srcReg);
                *os << "$" << offset << ", %r" << (int)srcReg;
            }
            break;

            case MDST_INDEX:
            {
                uint16 index;
                bs.Read(&index);
                uint8 srcReg;
                bs.Read(&srcReg);
                *os << "#" << index << ", %r" << (int)srcReg;
            }
            break;

            case MDST_STATIC:
            {
                uint16 index;
                bs.Read(&index);
                uint8 srcReg;
                bs.Read(&srcReg);
                *os << "static[" << index << "], %r" << (int)srcReg;
            }
            break;

            case MDST_REGISTER:
                switch (srcType)
                {
                case MSRC_REGISTER:
                {
                    uint8 dstReg;
                    bs.Read(&dstReg);
                    uint8 srcReg;
                    bs.Read(&srcReg);
                    *os << "%r" << (int)dstReg << ", %r" << (int)srcReg;
                }
                break;

                case MSRC_ARRAYIDX:
                {
                    uint8 arrayReg;
                    bs.Read(&arrayReg);
                    uint32 index;
                    bs.Read(&index);
                    uint8 srcReg;
                    bs.Read(&srcReg);
                    *os << "%r" << (int)arrayReg << "[" << index << "], %r" << (int)srcReg;
                }
                break;

                case MSRC_ARRAYIDX_REG:
                {
                    uint8 arrayReg;
                    bs.Read(&arrayReg);
                    uint8 indexReg;
                    bs.Read(&indexReg);
                    uint8 srcReg;
                    bs.Read(&srcReg);
                    *os << "%r" << (int)arrayReg << "[%r" << (int)indexReg << "], %r" << (int)srcReg;
                }
                break;

                case MSRC_MEMBER:
                {
                    uint8 objReg;
                    bs.Read(&objReg);
                    uint64 hash;
                    bs.Read(&hash);
                    uint8 srcReg;
                    bs.Read(&srcReg);
                    *os << "%r" << (int)objReg << ".member@" << std::hex << hash << std::dec << ", %r" << (int)srcReg;
                }
                break;

                case MSRC_TO_ARRAYIDX:
                {
                    uint8 arrayReg;
                    bs.Read(&arrayReg);
                    uint32 index;
                    bs.Read(&index);
                    uint8 srcReg;
                    bs.Read(&srcReg);
                    *os << "%r" << (int)arrayReg << "[" << index << "], %r" << (int)srcReg;
                }
                break;
                }
                break;
            }

            *os << std::endl;
        }

        break;
    }

    case CAST_UNIFIED:
    {
        uint8 subcmd;
        bs.Read(&subcmd);

        uint8 dstReg;
        bs.Read(&dstReg);
        uint8 srcReg;
        bs.Read(&srcReg);

        const uint8 castType = GET_CAST_TYPE(subcmd);

        if (os != nullptr)
        {
            *os << "cast_unified ";

            switch (castType)
            {
            case CAST_TYPE_U8:
                *os << "u8";
                break;
            case CAST_TYPE_U16:
                *os << "u16";
                break;
            case CAST_TYPE_U32:
                *os << "u32";
                break;
            case CAST_TYPE_U64:
                *os << "u64";
                break;
            case CAST_TYPE_I8:
                *os << "i8";
                break;
            case CAST_TYPE_I16:
                *os << "i16";
                break;
            case CAST_TYPE_I32:
                *os << "i32";
                break;
            case CAST_TYPE_I64:
                *os << "i64";
                break;
            case CAST_TYPE_F32:
                *os << "f32";
                break;
            case CAST_TYPE_F64:
                *os << "f64";
                break;
            case CAST_TYPE_BOOL:
                *os << "bool";
                break;
            case CAST_TYPE_DYNAMIC:
                *os << "dynamic";
                break;
            }

            *os << " %r" << (int)dstReg << ", %r" << (int)srcReg << std::endl;
        }

        break;
    }

    case NOP:
    {
        if (os != nullptr)
        {
            (*os) << "NOP" << std::endl;
        }

        break;
    }
    case LOAD_I32:
    {
        uint8 reg;
        bs.Read(&reg);

        int32_t val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_I32 ["
                << "%" << (int)reg << ", "
                << "i32(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_I64:
    {
        uint8 reg;
        bs.Read(&reg);

        int64_t val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_I64 ["
                << "%" << (int)reg << ", "
                << "i64(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_U32:
    {
        uint8 reg;
        bs.Read(&reg);

        uint32 val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_U32 ["
                << "%" << (int)reg << ", "
                << "u32(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_U64:
    {
        uint8 reg;
        bs.Read(&reg);

        uint64 val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_U64 ["
                << "%" << (int)reg << ", "
                << "u64(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_F32:
    {
        uint8 reg;
        bs.Read(&reg);

        float val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_F32 ["
                << "%" << (int)reg << ", "
                << "f32(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_F64:
    {
        uint8 reg;
        bs.Read(&reg);

        double val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_F64 ["
                << "%" << (int)reg << ", "
                << "f64(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_OFFSET:
    {
        uint8 reg;
        bs.Read(&reg);

        uint16 offset;
        bs.Read(&offset);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_OFFSET ["
                << "%" << (int)reg << ", "
                                      "$(sp-"
                << offset << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_INDEX:
    {
        uint8 reg;
        bs.Read(&reg);

        uint16 idx;
        bs.Read(&idx);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_INDEX ["
                << "%" << (int)reg << ", "
                                      "u16("
                << idx << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_STATIC:
    {
        uint8 reg;
        bs.Read(&reg);

        uint16 index;
        bs.Read(&index);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_STATIC ["
                << "%" << (int)reg << ", "
                << "#" << index
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_STRING:
    {
        uint8 reg;
        bs.Read(&reg);

        // get string length
        uint32 len;
        bs.Read(&len);

        // read string based on length
        char* str = new char[len + 1];
        bs.Read(str, len);
        str[len] = '\0';

        if (os != nullptr)
        {
            (*os)
                << "LOAD_STRING ["
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
        uint8 reg;
        bs.Read(&reg);

        uint32 val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os) << "LOAD_ADDR [%" << (int)reg << ", @(" << std::hex << val << std::dec << ")]" << std::endl;
        }

        break;
    }
    case LOAD_FUNC:
    {
        uint8 reg;
        bs.Read(&reg);

        uint32 addr;
        bs.Read(&addr);

        uint8 nargs;
        bs.Read(&nargs);

        uint8 flags;
        bs.Read(&flags);

        if (os != nullptr)
        {
            (*os) << "LOAD_FUNC [%" << (int)reg
                  << ", @(" << std::hex << addr << std::dec << "), "
                  << "u8(" << (int)nargs << ")], "
                  << "u8(" << (int)flags << ")]"
                  << std::endl;
        }

        break;
    }
    case GET_MEMBER:
    {
        uint8 reg;
        bs.Read(&reg);

        uint8 src;
        bs.Read(&src);

        uint64 hash;
        bs.Read(&hash);

        if (os != nullptr)
        {
            (*os)
                << "GET_MEMBER ["
                << "%" << (int)reg << ", "
                << "%" << (int)src << ", "
                << "u64(" << hash << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_ARRAYIDX:
    {
        uint8 reg;
        bs.Read(&reg);

        uint8 src;
        bs.Read(&src);

        uint8 idx;
        bs.Read(&idx);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_ARRAYIDX ["
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
        uint8 reg;
        bs.Read(&reg);

        uint16 offset;
        bs.Read(&offset);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_OFFSET_REF ["
                << "%" << (int)reg << ", "
                                      "$(sp-"
                << offset << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_INDEX_REF:
    {
        uint8 reg;
        bs.Read(&reg);

        uint16 idx;
        bs.Read(&idx);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_INDEX_REF ["
                << "%" << (int)reg << ", "
                                      "u16("
                << idx << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case REF:
    {
        uint8 srcReg;
        uint8 dstReg;

        bs.Read(&srcReg);
        bs.Read(&dstReg);

        if (os != nullptr)
        {
            (*os)
                << "REF ["
                << "%" << (int)dstReg << ", "
                << "%" << (int)srcReg
                << "]"
                << std::endl;
        }

        break;
    }
    case DEREF:
    {
        uint8 srcReg;
        uint8 dstReg;

        bs.Read(&srcReg);
        bs.Read(&dstReg);

        if (os != nullptr)
        {
            (*os)
                << "DEREF ["
                << "%" << (int)dstReg << ", "
                << "%" << (int)srcReg
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_NULL:
    {
        uint8 reg;
        bs.Read(&reg);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_NULL ["
                << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_TRUE:
    {
        uint8 reg;
        bs.Read(&reg);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_TRUE ["
                << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_FALSE:
    {
        uint8 reg;
        bs.Read(&reg);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_FALSE ["
                << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case LOAD_CLASS:
    {
        uint8 reg;
        bs.Read(&reg);

        uint64 nameHash;
        bs.Read(&nameHash);

        if (os != nullptr)
        {
            (*os)
                << "LOAD_CLASS ["
                << "%" << (int)reg << ", "
                << "u64(" << nameHash << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_OFFSET:
    {
        uint16 dst;
        bs.Read(&dst);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "MOV_OFFSET ["
                << "$(sp-" << dst << "), "
                << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_INDEX:
    {
        uint16 dst;
        bs.Read(&dst);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "MOV_INDEX ["
                << "u16(" << dst << "), "
                << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_STATIC:
    {
        uint16 dst;
        bs.Read(&dst);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "MOV_STATIC ["
                << "#" << dst << ", "
                << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case SET_FIELD:
    {
        uint8 reg;
        bs.Read(&reg);

        uint64 hash;
        bs.Read(&hash);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "SET_FIELD ["
                << "%" << (int)reg << ", "
                << "u64(" << hash << "), "
                << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV_ARRAYIDX:
    {
        uint8 reg;
        bs.Read(&reg);

        uint32 idx;
        bs.Read(&idx);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "MOV_ARRAYIDX ["
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
        uint8 reg;
        bs.Read(&reg);

        uint8 idx;
        bs.Read(&idx);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "MOV_ARRAYIDX_REG ["
                << "%" << (int)reg << ", "
                << "%" << (int)idx << ", "
                << "%" << (int)src << ""
                << "]"
                << std::endl;
        }

        break;
    }
    case MOV:
    {
        uint8 dst;
        bs.Read(&dst);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "MOV ["
                << "%" << (int)dst << ", "
                << "%" << (int)src << ""
                << "]"
                << std::endl;
        }

        break;
    }
    case CHECK_HAS_MEMBER:
    {
        uint8 reg;
        bs.Read(&reg);

        uint8 src;
        bs.Read(&src);

        uint64 hash;
        bs.Read(&hash);

        if (os != nullptr)
        {
            (*os)
                << "CHECK_HAS_MEMBER ["
                << "%" << (int)reg << ", "
                << "%" << (int)src << ", "
                << "u64(" << hash << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case PUSH:
    {
        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "PUSH ["
                << "%" << (int)src
                << "]"
                << std::endl;
        }

        break;
    }
    case POP:
    {
        if (os != nullptr)
        {
            (*os)
                << "POP"
                << std::endl;
        }

        break;
    }
    case PUSH_ARRAY:
    {
        uint8 dst;
        bs.Read(&dst);

        uint8 src;
        bs.Read(&src);

        if (os != nullptr)
        {
            (*os)
                << "PUSH_ARRAY ["
                << "% " << (int)dst << ", "
                << "% " << (int)src
                << "]" << std::endl;
        }

        break;
    }
    case ADD_SP:
    {
        uint16 val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "ADD_SP ["
                << "u16(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case SUB_SP:
    {
        uint16 val;
        bs.Read(&val);

        if (os != nullptr)
        {
            (*os)
                << "SUB_SP ["
                << "u16(" << val << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JMP:
    {
        uint32 addr;
        bs.Read(&addr);

        if (os != nullptr)
        {

            (*os)
                << "JMP ["
                << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JE:
    {
        uint32 addr;
        bs.Read(&addr);

        if (os != nullptr)
        {
            (*os)
                << "JE ["
                << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JNE:
    {
        uint32 addr;
        bs.Read(&addr);

        if (os != nullptr)
        {
            (*os)
                << "JNE ["
                << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JG:
    {
        uint32 addr;
        bs.Read(&addr);

        if (os != nullptr)
        {
            (*os)
                << "JG ["
                << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case JGE:
    {
        uint32 addr;
        bs.Read(&addr);

        if (os != nullptr)
        {
            (*os)
                << "JGE ["
                << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case CALL:
    {
        uint8 func;
        bs.Read(&func);

        uint8 argc;
        bs.Read(&argc);

        if (os != nullptr)
        {
            (*os)
                << "CALL ["
                << "%" << (int)func << ", "
                << "u8(" << (int)argc << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case RET:
    {
        if (os != nullptr)
        {
            (*os)
                << "RET"
                << std::endl;
        }

        break;
    }
    case BEGIN_TRY:
    {
        uint32 addr;
        bs.Read(&addr);

        if (os != nullptr)
        {
            (*os)
                << "BEGIN_TRY ["
                << "@(" << std::hex << addr << std::dec << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case END_TRY:
    {
        if (os != nullptr)
        {
            (*os) << "END_TRY" << std::endl;
        }

        break;
    }
    case NEW:
    {
        uint8 dst;
        bs.Read(&dst);

        uint8 type;
        bs.Read(&type);

        if (os != nullptr)
        {
            (*os)
                << "NEW ["
                << "%" << (int)dst << ", "
                << "%" << (int)type
                << "]"
                << std::endl;
        }

        break;
    }
    case NEW_ARRAY:
    {
        uint8 dst;
        bs.Read(&dst);

        uint32 size;
        bs.Read(&size);

        if (os != nullptr)
        {
            (*os)
                << "NEW_ARRAY ["
                << "%" << (int)dst << ", "
                << "u32(" << (int)size << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case BEGIN_CLASS:
    {
        uint8 reg;
        bs.Read(&reg);

        // Read class name length and name
        uint16 nameLen;
        bs.Read(&nameLen);

        char* nameStr = new char[nameLen + 1];
        nameStr[nameLen] = '\0';
        bs.Read(nameStr, nameLen);

        // Read type id
        TypeId::ValueType typeIdValue; // keep same type as VM
        bs.Read(&typeIdValue);

        uint8 flags;
        bs.Read(&flags);

        if (os != nullptr)
        {
            (*os) << "BEGIN_CLASS [%" << (int)reg << ", str(" << nameStr << "), u64(" << typeIdValue << "), u8(" << (int)flags
                  << ")]" << std::endl;
        }

        bool hitEnd = false;

        // Read members until we hit END_CLASS
        while (!bs.Eof() && !hitEnd)
        {
            uint8 nextByte;
            bs.Read(&nextByte);

            if (nextByte == Instructions::END_CLASS)
            {
                if (os != nullptr)
                {
                    (*os) << "END_CLASS" << std::endl;
                }
                hitEnd = true;
                break;
            }

            uint8 memberType = nextByte;

            // Read member count
            uint16 memberCount;
            bs.Read(&memberCount);

            if (os != nullptr)
            {
                const char* memberTypeStr = "unknown";

                switch (memberType)
                {
                case (uint8)HypMemberType::TYPE_CONSTANT:
                    memberTypeStr = "constant";
                    break;
                case (uint8)HypMemberType::TYPE_PROPERTY:
                    memberTypeStr = "property";
                    break;
                case (uint8)HypMemberType::TYPE_FIELD:
                    memberTypeStr = "field";
                    break;
                case (uint8)HypMemberType::TYPE_METHOD:
                    memberTypeStr = "method";
                    break;
                default:
                    break;
                }

                (*os) << "\tmembers [type(" << memberTypeStr << "), count(" << memberCount << ")]" << std::endl;
            }

            // Read each member
            for (uint16 i = 0; i < memberCount; i++)
            {
                // Read member name
                uint16 memberNameLen;
                bs.Read(&memberNameLen);

                char* memberNameStr = new char[memberNameLen + 1];
                memberNameStr[memberNameLen] = '\0';
                bs.Read(memberNameStr, memberNameLen);

                // Read attributes
                uint16 numAttrs;
                bs.Read(&numAttrs);

                if (os != nullptr)
                {
                    (*os) << "\t\tmember [name(" << memberNameStr << "), attrs(" << numAttrs << ")]" << std::endl;
                }

                // Read each attribute
                for (uint16 attrIdx = 0; attrIdx < numAttrs; attrIdx++)
                {
                    // Read attribute name
                    uint16 attrNameLen;
                    bs.Read(&attrNameLen);

                    char* attrNameStr = new char[attrNameLen + 1];
                    attrNameStr[attrNameLen] = '\0';
                    bs.Read(attrNameStr, attrNameLen);

                    // Read attribute type
                    uint8 attrType;
                    bs.Read(&attrType);

                    if (os != nullptr)
                    {
                        (*os) << "\t\t\tattr [name(" << attrNameStr << "), type(" << (int)attrType << ")";
                    }

                    switch ((HypClassAttributeType)attrType)
                    {
                    case HypClassAttributeType::STRING:
                    {
                        uint32 strLen;
                        bs.Read(&strLen);

                        char* strData = new char[strLen + 1];
                        strData[strLen] = '\0';
                        bs.Read(strData, strLen);

                        if (os != nullptr)
                        {
                            (*os) << ", value(\"" << strData << "\")";
                        }

                        delete[] strData;
                        break;
                    }
                    case HypClassAttributeType::INT:
                    {
                        int32 iValue;
                        bs.Read(&iValue);

                        if (os != nullptr)
                        {
                            (*os) << ", value(" << iValue << ")";
                        }
                        break;
                    }
                    case HypClassAttributeType::BOOLEAN:
                    {
                        uint8 bValue;
                        bs.Read(&bValue);

                        if (os != nullptr)
                        {
                            (*os) << ", value(" << (bValue ? "true" : "false") << ")";
                        }
                        break;
                    }
                    default:
                        break;
                    }

                    if (os != nullptr)
                    {
                        (*os) << "]" << std::endl;
                    }

                    delete[] attrNameStr;
                }

                // Read member type id (same as VM)
                TypeId::ValueType memberTypeIdValue;
                bs.Read(&memberTypeIdValue);

                // Read member-type-specific data (mirror VM)
                switch ((HypMemberType)memberType)
                {
                case HypMemberType::TYPE_FIELD:
                {
                    TypeId::ValueType targetTypeIdValue;
                    bs.Read(&targetTypeIdValue);

                    uint32 offset;
                    bs.Read(&offset);

                    uint32 size;
                    bs.Read(&size);

                    if (os != nullptr)
                    {
                        (*os) << "\t\t\tfield [typeId(" << memberTypeIdValue << "), targetTypeId(" << targetTypeIdValue
                              << "), offset(" << offset << "), size(" << size << ")]" << std::endl;
                    }
                    break;
                }
                case HypMemberType::TYPE_METHOD:
                {
                    TypeId::ValueType targetTypeIdValue;
                    bs.Read(&targetTypeIdValue);

                    uint8 flags;
                    bs.Read(&flags);

                    uint16 stackOffset;
                    bs.Read(&stackOffset);

                    if (os != nullptr)
                    {
                        (*os) << "\t\t\tmethod [typeId(" << memberTypeIdValue << "), targetTypeId(" << targetTypeIdValue
                              << "), flags(" << (int)flags << "), stackOffset(" << stackOffset << ")]" << std::endl;
                    }
                    break;
                }
                case HypMemberType::TYPE_PROPERTY:
                {
                    if (os != nullptr)
                    {
                        (*os) << "\t\t\tproperty [typeId(" << memberTypeIdValue << ")]" << std::endl;
                    }
                    break;
                }
                case HypMemberType::TYPE_CONSTANT:
                {
                    uint32 valueSize;
                    bs.Read(&valueSize);

                    if (os != nullptr)
                    {
                        (*os) << "\t\t\tconstant [typeId(" << memberTypeIdValue << "), size(" << valueSize << ")]" << std::endl;
                    }
                    break;
                }
                default:
                    break;
                }

                delete[] memberNameStr;
            }
        }

        if (!hitEnd)
        {
            if (os != nullptr)
            {
                (*os) << "warning: BEGIN_CLASS missing END_CLASS before EOF" << std::endl;
            }
        }

        delete[] nameStr;

        break;
    }
    case CMP:
    {
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        if (os != nullptr)
        {
            (*os)
                << "CMP ["
                << "%" << (int)lhs << ", "
                << "%" << (int)rhs
                << "]"
                << std::endl;
        }

        break;
    }
    case CMPZ:
    {
        uint8 lhs;
        bs.Read(&lhs);

        if (os != nullptr)
        {
            (*os)
                << "CMPZ ["
                << "%" << (int)lhs
                << "]"
                << std::endl;
        }

        break;
    }
    case ADD:
    {
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "ADD ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "SUB ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "MUL ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "DIV ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "MOD ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "AND ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "OR ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "XOR ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "SHL ["
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
        uint8 lhs;
        bs.Read(&lhs);

        uint8 rhs;
        bs.Read(&rhs);

        uint8 dst;
        bs.Read(&dst);

        if (os != nullptr)
        {
            (*os)
                << "SHR ["
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
        uint8 reg;
        bs.Read(&reg);

        if (os != nullptr)
        {
            (*os)
                << "NEG ["
                << "%" << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case THROW:
    {
        uint8 reg;
        bs.Read(&reg);

        if (os != nullptr)
        {
            (*os)
                << "THROW ["
                << "% " << (int)reg
                << "]"
                << std::endl;
        }

        break;
    }
    case TRACEMAP:
    {
        uint32 len;
        bs.Read(&len);

        if (os != nullptr)
        {
            (*os)
                << "TRACEMAP ["
                << "u32(" << len << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case REM:
    {
        uint32 len;
        bs.Read(&len);

        // read string based on length
        char* str = new char[len + 1];
        bs.Read(str, len);
        str[len] = '\0';

        if (os != nullptr)
        {
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
        uint8 reg;
        bs.Read(&reg);

        uint64 hash;
        bs.Read(&hash);

        if (os != nullptr)
        {
            (*os)
                << "EXPORT ["
                << "%" << (int)reg << ", "
                << "u64(" << hash << ")"
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U8:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_U8 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U16:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_U16 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U32:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_U32 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_U64:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_U64 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I8:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_I8 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I16:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_I16 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I32:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_I32 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_I64:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_I64 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_F32:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_F32 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_F64:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_F64 ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_BOOL:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_BOOL ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case CAST_DYNAMIC:
    {
        uint8 regDst;
        bs.Read(&regDst);

        uint8 regSrc;
        bs.Read(&regSrc);

        if (os != nullptr)
        {
            (*os)
                << "CAST_DYNAMIC ["
                << "%" << (int)regDst << ", "
                << "%" << (int)regSrc
                << "]"
                << std::endl;
        }

        break;
    }
    case EXIT:
    {
        if (os != nullptr)
        {
            (*os)
                << "EXIT"
                << std::endl;
        }

        break;
    }
    default:
        if (os != nullptr)
        {
            (*os)
                << "??"
                << std::endl;
        }
        // unrecognized instruction

        break;
    }
}

InstructionStream DecompilationUnit::Decompile(hyperion::vm::BytecodeStream& bs, std::ostream* os)
{
    const SizeType prevPosition = bs.Position();

    InstructionStream is;

    while (!bs.Eof())
    {
        const SizeType pos = bs.Position();

        if (os != nullptr)
        {
            (*os) << std::hex << pos << std::dec << "\t";
        }

        uint8 code;
        bs.Read(&code);

        DecodeNext(code, bs, is, os);
    }

    // rewind back to where we started
    bs.Seek(prevPosition);

    return is;
}

} // namespace hyperion::compiler
