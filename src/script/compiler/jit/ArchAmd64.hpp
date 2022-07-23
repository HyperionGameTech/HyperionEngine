#ifndef HYPERION_JIT_AMD64_HPP
#define HYPERION_JIT_AMD64_HPP


namespace hyperion::compiler {

enum RegisterN
{
    /**************************/
    /* 16 bit WORD registers  */
    /**************************/
    R_AX,
    R_CX,
    R_DX,
    R_BX,
    R_SI,
    R_DI,
    R_SP,
    R_BP,
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
    R_ESI,
    R_EDI,
    R_ESP,
    R_EBP,
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
    R_RSP,
    R_RBP,
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

#define REG_IS_NOT_EXT(R) \
    (R < R_R8D && (R < R_R8W || R > R_R15W))

class CompilerAMD64
{
    using InstrBuffer = std::vector<uint8_t>;

    struct InstrArg
    {
        enum class Type
        {
            IMMEDIATE,
            REGISTER,
        };
        Type type = Type::IMMEDIATE;

        uint64_t GetArg()
        {
            
        }

        union
        {
            uint64_t value;
            RegisterN reg_n;
        };
    };

    bool RegIs64(RegisterN r) { return (r >= R_R15D); }

    bool RegIsExtended(RegisterN r)
    {
        return (RegIs64(r) || (r <= R_R15D && r >= R_R8D) || (r <= R_R15W && r >= R_R8W));
    }

    uint8_t MakeSIB(uint8_t scaled_index, uint8_t mode, RegisterN base_reg)
    {
        return 0;
        //return (mode << 6) | (scaled_index << 3) | 
    }

    uint8_t MakeREX(RegisterN rsrc, RegisterN rdest, bool sib)
    {
        bool is_64 = RegIs64(rdest);
        uint8_t rex;
        /* Set lower 4 bits of REX for WRXB */
        rex = (is_64 << 3) | (RegIsExtended(rdest) << 2) | (sib << 1) | RegIsExtended(rsrc);
        rex = (rex & 0x0F) | 0x40; /* Set the high 4 bits to 0x04 */
        return rex;
    }
    InstrBuffer BuildMovInstr(RegisterN rsrc, RegisterN rdest)
    {
        InstrBuffer instr;
        instr = {
            MakeREX(rsrc, rdest, false),
        };
    }
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


struct Instruction
{
    std::array<uint8_t, 4> prefix;
    std::array<uint8_t, 3> opcode;
    uint8_t modrm, sib;
};


class CompilerOutput()
{



}




}







#endif