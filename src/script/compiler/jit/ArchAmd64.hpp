#ifndef HYPERION_JIT_AMD64_HPP
#define HYPERION_JIT_AMD64_HPP

#include <script/compiler/jit/JitBuffer.hpp>

#include <vector>
#include <cstdint>


namespace hyperion::compiler::jit {

#define VALUE_IS_BYTE(d) ((d) && ((d) < 0x7F && (d) > -0x7F))

enum RegisterNum
{
    R_NONE,
    /**************************/
    /* 16 bit WORD registers  */
    /**************************/
    R_AX,
    R_CX,
    R_DX,
    R_BX,
    R_SP,
    R_BP,
    R_SI,
    R_DI,
    /* Extended */
    R_R8W,
    R_R9W,
    R_R10W,
    R_R11W,
    R_R12W,
    R_R13W,
    R_R14W,
    R_R15W,

    /**************************/
    /* 32 bit DWORD registers */
    /**************************/
    R_EAX,
    R_ECX,
    R_EDX,
    R_EBX,
    R_ESP,
    R_EBP,
    R_ESI,
    R_EDI,
    /* Extended */
    R_R8D,
    R_R9D,
    R_R10D,
    R_R11D,
    R_R12D,
    R_R13D,
    R_R14D,
    R_R15D,

    /**************************/
    /* 64 bit QWORD registers */
    /**************************/
    R_RAX,
    R_RCX,
    R_RDX,
    R_RBX,
    R_RSP,
    R_RBP,
    R_RSI,
    R_RDI,
    /* Extended */
    R_R8,
    R_R9,
    R_R10,
    R_R11,
    R_R12,
    R_R13,
    R_R14,
    R_R15
};

class JitRegister {
public:
    JitRegister(RegisterNum reg)
        : m_reg(reg) { }

    bool Is64bit() { return (GetRegister() >= R_RAX); }
    bool Is32bit() { return (GetRegister() >= R_EAX) && !(Is64bit()); }
    bool Is16bit() { return (GetRegister() < R_EAX && GetRegister() != R_NONE); }
    bool IsExtended()
    {
        const uint8_t r = GetRegister();
        return ((r >= R_R8) || (r <= R_R15D && r >= R_R8D) || (r <= R_R15W && r >= R_R8W));
    }

    RegisterNum GetRegister() { return m_reg; }

private:
    RegisterNum m_reg = R_NONE;
};



struct PassableValue {
    enum Type {
        INT64,
        REGISTER,
        POINTER,
    };
    union Data {
        Data(uint64_t _i64)    : i64(_i64) { }
        Data(void *_ptr)       : ptr(_ptr) { }
        Data(JitRegister _reg) : reg(_reg) { }

        uint64_t i64;
        void *ptr;
        JitRegister reg;
    };
    PassableValue(uint64_t i64)
        : m_type(Type::INT64), m_data(i64) { }
    PassableValue(JitRegister reg)
        : m_type(Type::REGISTER), m_data(reg) { }
    PassableValue(void *ptr)
        : m_type(Type::POINTER), m_data(ptr) { }

    Type GetType() { return m_type; }

    JitRegister GetRegister() { return m_data.reg; }
    void *GetPointer() { return m_data.ptr; }

    int64_t GetInteger() {
        if (m_type == INT64)
            return m_data.i64;
        return 0;
    }
    uint64_t GetUnsigned() { return (uint64_t)GetInteger(); }

    Type m_type = INT64;
    Data m_data;
};

#define REG_IS_NOT_EXT(R) \
    ((R) < R_R8D && ((R) < R_R8W || (R) > R_R15W))

class CompilerAMD64
{
public:
    JitRegister r_none = JitRegister(R_NONE);
    JitRegister r_rax = JitRegister(R_RAX); /* accumulator */
    JitRegister r_rcx = JitRegister(R_RCX); /* count */
    JitRegister r_rdx = JitRegister(R_RDX); /* io addressing */
    JitRegister r_rbx = JitRegister(R_RBX); /* base */
    JitRegister r_rsi = JitRegister(R_RSI); /* source index */
    JitRegister r_rdi = JitRegister(R_RDI); /* destination index */
    JitRegister r_rsp = JitRegister(R_RSP); /* stack pointer */
    JitRegister r_rbp = JitRegister(R_RBP); /* base pointer */

    JitRegister r_r8 = JitRegister(R_R8); /* base pointer */
    JitRegister r_r9 = JitRegister(R_R9); /* base pointer */

    uint8_t MakeSIB(uint8_t scale, JitRegister index, JitRegister base)
    {
        uint8_t sib = 0;
        sib = ((scale & 0x03) << 6) | ((GetBaseReg(index) & 0x07) << 3) | (GetBaseReg(base) & 0x07);
        return sib;
    }

    uint8_t GetBaseReg(JitRegister reg)
    {
        uint8_t regn = reg.GetRegister();
        if (regn >= R_RAX)
            return regn-R_RAX;
        if (regn >= R_EAX)
            return regn-R_EAX;
        return regn;
    }

    InstructionBuffer MakeModRM(PassableValue src, PassableValue dest, int32_t src_disp=0, int32_t dest_disp=0)
    {
        /*************************/
        /* [7 6] [5 4 3] [2 1 0] */
        /* [mod] [ reg ] [ r\m ] */
        /*************************/
        uint8_t mod = 0x03;
        uint8_t final = 0;

        InstructionBuffer instr;

        if (dest.GetType() == PassableValue::POINTER)
            mod = 0x00;
        /* Check if the displacement is 8 bit */
        else if (VALUE_IS_BYTE(dest_disp) || VALUE_IS_BYTE(src_disp))
            mod = 0x01;
        /* The displacement must be greater, we will switch to 32 bit */
        else if (dest_disp != 0 || src_disp != 0)
            mod = 0x02;
        /* Set the mod */
        final |= (mod << 6);

        uint8_t reg = 0;

        uint8_t rm = 0;
        InstructionBuffer post;

        if (dest_disp != 0 || src_disp != 0) {
            JitRegister jregister = src.GetRegister();

            RegisterNum regn = jregister.GetRegister();

            if ((regn >= R_RAX && regn <= R_RBX) || (regn >= R_RBP && regn <= R_RDI)) {
                /* R/M 0-4 is reserved for our registers */
                if (dest_disp) {
                    reg = GetBaseReg(jregister);
                    rm = GetBaseReg(dest.GetRegister());
                }
                else {
                    reg = GetBaseReg(dest.GetRegister());
                    rm = GetBaseReg(jregister);
                }
            }
            else {
                if (dest_disp) {
                    reg = 0x04; /* Generate a SIB byte */
                    rm = GetBaseReg(dest.GetRegister());
                }
                else {
                    reg = GetBaseReg(dest.GetRegister());
                    rm = 0x04; /* Generate a SIB byte */
                }
                post.push_back(MakeSIB(0, r_rsp, r_rbp));
            }
            goto rmskip;
        }

        if (src.GetType() == PassableValue::POINTER) {
            RegisterNum regn = src.GetRegister().GetRegister();
            /* We can avoid having to generate a SIB if our register is rax, rcx, rdx, rbx, rsi, or rdi.. */
            if ((regn >= R_RAX && regn <= R_RBX) || (regn == R_RSI || regn == R_RDI)) {
                /* R/M 0-4 is reserved for our registers */
                reg = GetBaseReg(src.GetRegister());
            }
            else {
                reg = 0x04; /* Generate that bad boy */
                post.push_back(MakeSIB(0, r_rsp, r_rbp));
            }
        }
        else if (src.GetType() == PassableValue::REGISTER) {
            reg = GetBaseReg(src.GetRegister());
        }

        if (dest.GetType() == PassableValue::POINTER) {
            RegisterNum regn = dest.GetRegister().GetRegister();
            /* We can avoid having to generate a SIB if our register is rax, rcx, rdx, rbx, rsi, or rdi.. */
            if ((regn >= R_RAX && regn <= R_RBX) || (regn == R_RSI || regn == R_RDI)) {
                /* R/M 0-4 is reserved for our registers */
                rm = GetBaseReg(dest.GetRegister());
            }
            else {
                rm = 0x04; /* Generate that bad boy */
                post.push_back(MakeSIB(0, r_rsp, r_rbp));
            }
        }
        else if (dest.GetType() == PassableValue::REGISTER) {
            rm = (GetBaseReg(dest.GetRegister()));
        }
    rmskip:;
        /* Add in the reg field */
        final |= ((reg & 0x07) << 3);
        /* The R/M field is the final 3 bits, so we mask with 0b111 */
        final |= (rm & 0x07);

        instr.push_back(final);
        instr.insert_back(post);

        return instr;
    }

    uint8_t MakeREX(JitRegister src, JitRegister dest, bool sib)
    {
        uint8_t rex;
        /* Set lower 4 bits of REX for WRXB */
        rex = (dest.Is64bit() << 3) | (dest.IsExtended() << 2) | (sib << 1) | src.IsExtended();
        rex = (rex & 0x0F) | 0x40; /* Set the high 4 bits to 0x04 */
        return rex;
    }

    InstructionBuffer BuildMov64(PassableValue src, PassableValue dest, int32_t src_disp=0, int32_t dest_disp=0)
    {
        InstructionBuffer instr = { };
        instr.push_back(
            MakeREX(
                (src.GetType() == PassableValue::REGISTER) ? src.GetRegister() : r_none,
                dest.GetRegister(),
                false
            )
        );
        switch (src.GetType()) {
            case PassableValue::INT64:
                instr.push_back(0xC7);
                break;
            case PassableValue::POINTER:// fallthrough
            case PassableValue::REGISTER:
                if (src_disp != 0) {
                    instr.push_back(0x8B);
                    break;
                }
                instr.push_back(0x89);
                break;
            default:;
        }
        instr.insert_back(MakeModRM(src, dest, src_disp, dest_disp));

        if (src.GetType() == PassableValue::INT64) {
            uint32_t imm = src.GetInteger();
            instr.push_back(imm);
        }
        else if (dest_disp != 0) {
            if (VALUE_IS_BYTE(dest_disp))
                instr.push_back((uint8_t)dest_disp);
            else
                instr.push_back(dest_disp);
        }
        else if (src_disp != 0) {
            if (VALUE_IS_BYTE(src_disp))
                instr.push_back((uint8_t)src_disp);
            else
                instr.push_back(src_disp);
        }

        return instr;
    }

    InstructionBuffer BuildJmp64(uint8_t near_ip)
    {
        InstructionBuffer instr;
        instr = {
            0xEB,
            near_ip
        };
        return instr;
    }
    InstructionBuffer BuildAdd64(JitRegister reg, PassableValue imm)
    {
        InstructionBuffer instr;
        instr = {
            MakeREX((imm.GetType() == PassableValue::REGISTER) ? imm.GetRegister() : r_none, reg, false),
        };
        switch (imm.GetType()) {
            case PassableValue::REGISTER:
                instr.push_back(0x01);
                break;
            case PassableValue::POINTER:
                instr.push_back(0x03);
                break;
            case PassableValue::INT64:
                instr.push_back(0x83);
                break;
        }
        instr.insert_back(MakeModRM(imm, reg));

        if (imm.GetType() == PassableValue::INT64)
            instr.push_back((int8_t)imm.GetInteger());
        else if (imm.GetType() == PassableValue::POINTER)
            instr.push_back((int32_t)imm.GetInteger());
        return instr;
    }

    InstructionBuffer BuildPush64(PassableValue value)
    {
        InstructionBuffer instr = { };

        if (value.GetType() == PassableValue::REGISTER) {
            JitRegister reg = value.GetRegister();
            if (reg.IsExtended())
                instr.push_back(MakeREX(r_none, reg, false));
            instr.push_back(((0x05 << 4) | GetBaseReg(reg)));
        }
        else if (value.GetType() == PassableValue::INT64) {
            uint64_t v64 = value.GetInteger();
            if (VALUE_IS_BYTE(v64)) {
                instr = { 0x6A, (uint8_t)v64 };
                return instr;
            }
        }

        return instr;
    }

    InstructionBuffer BuildPop64(JitRegister reg)
    {
        InstructionBuffer instr = { };
        if (reg.IsExtended())
            instr.push_back(MakeREX(r_none, reg, false));
        instr.push_back(((0x05 << 4) | 0x08 | (GetBaseReg(reg) & 0x07)));
        return instr;
    }

    InstructionBuffer StartFunction()
    {
        InstructionBuffer instr = {
            BuildPush64(r_rbp),
            BuildMov64(r_rsp, r_rbp)
        };
        return instr;
    }

    InstructionBuffer EndFunction()
    {
        InstructionBuffer instr = {
            BuildPop64(r_rbp),
            BuildRet()
        };
        return instr;
    }

    InstructionBuffer BuildCall64(uint32_t near_offset) {
        InstructionBuffer instr = {
            0xE8,
            /* immediate data */
            (uint8_t)(near_offset & 0xFF),
            (uint8_t)((near_offset >> 8)  & 0xFF),
            (uint8_t)((near_offset >> 16) & 0xFF),
            (uint8_t)((near_offset >> 24) & 0xFF),
        };
        return instr;
    }

    InstructionBuffer BuildArgUpload(PassableValue &arg) {
        InstructionBuffer instr = {};
        switch (arg.GetType()) {
            case PassableValue::REGISTER:
                break;
            case PassableValue::INT64:
                break;
            default:;
        }

        return instr;
    }

    InstructionBuffer BuildArgList(std::vector<PassableValue> &args) {
        InstructionBuffer instr = {};
        const JitRegister argtab[] = {
            r_rdi, r_rsi, r_rdx, r_rcx, r_r8, r_r9
        };

        for (int i = 0; i < args.size(); i++) {
            PassableValue *arg = &args[i];
            JitRegister arg_reg = argtab[i];

            if (arg->GetType() == PassableValue::INT64)
                instr += { BuildMov64(arg->GetInteger(), arg_reg) };
            else if (arg->GetType() == PassableValue::REGISTER)
                instr += { BuildMov64(arg->GetRegister(), arg_reg) };
        }
        return instr;
    }

    InstructionBuffer BuildArgRetrieveList(int size) {
        InstructionBuffer instr = {};
        const JitRegister argtab[] = {
            r_rdi, r_rsi, r_rdx, r_rcx, r_r8, r_r9
        };
        int offset = 0;

        for (int i = 0; i < size; i++) {
            JitRegister arg_reg = argtab[size-1-i];

            offset += 8;
            instr.insert_back(BuildMov64(arg_reg, r_rbp, 0, -offset));
        }
        return instr;
    }

    InstructionBuffer BuildRet() { return { 0xC3 }; }
};

enum class ArchInstr
{
    Ret,
    Mov,
    Add,
    Sub,
    Mul,
    IMul,
    Xor,
    Inc,
    Dec,
    Push,
    Pop,
    Call,
    Loop,
    Jmp,
    Syscall,

    InstrSize,
};




}


#endif