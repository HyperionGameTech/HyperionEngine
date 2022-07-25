#ifndef HYPERION_JIT_AMD64_HPP
#define HYPERION_JIT_AMD64_HPP

#include <vector>
#include <cstdint>

namespace hyperion::compiler {

enum RegisterN
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

#define REG_IS_NOT_EXT(R) \
    ((R) < R_R8D && ((R) < R_R8W || (R) > R_R15W))

class InstrBuffer : public std::vector<uint8_t> {
public:
    using vector<uint8_t>::vector;

    InstrBuffer(std::initializer_list<InstrBuffer> il) {
        for (auto &ib : il) {
            this->insert_back(ib);
        }
    }

    InstrBuffer operator += (InstrBuffer const &other)
    {
        this->insert_back(other);
        return *this;
    }
    void insert_back(InstrBuffer const &other)
    {
        this->insert(this->end(), other.begin(), other.end());
    }
};

class CompilerAMD64
{
public:


    bool RegIs64(RegisterN r) { return (r > R_R15D); }
    bool RegIs32(RegisterN r) { return (r >= R_EAX) && !(RegIs64(r)); }
    bool RegIs16(RegisterN r) { return (r < R_EAX); }

    bool RegIsExtended(RegisterN r)
    {
        return ((r >= R_R8) || (r <= R_R15D && r >= R_R8D) || (r <= R_R15W && r >= R_R8W));
    }

    uint8_t MakeSIB(uint8_t scaled_index, uint8_t mode, RegisterN base_reg)
    {
        return 0;
        //return (mode << 6) | (scaled_index << 3) | 
    }

    uint8_t GetBaseReg(RegisterN reg)
    {
        if (reg >= R_RAX)
            return reg-R_RAX;
        if (reg >= R_EAX)
            return reg-R_RAX;
        return reg;
    }

    uint8_t MakeModRM(bool src_is_reg, bool dest_is_reg, uint16_t srcv, uint16_t destv)
    {
        /*************************/
        /* [7 6] [5 4 3] [2 1 0] */
        /* [mod] [ reg ] [ r\m ] */
        /*************************/
        uint8_t mod = 0;

        mod |= (0x01 << 7);
        mod |= (0x01 << 6);

        if (src_is_reg) {
            mod |= ((GetBaseReg((RegisterN)srcv) & 0x07) << 3);
        }
        if (dest_is_reg) {
            mod |= (GetBaseReg((RegisterN)destv) & 0x07);
        }
        return mod;
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
    InstrBuffer BuildMov64(RegisterN rsrc, RegisterN rdest)
    {
        InstrBuffer instr;
        instr = {
            MakeREX(rsrc, rdest, false),
            0x89,
            MakeModRM(true, true, rsrc, rdest),
        };
        return instr;
    }
    InstrBuffer BuildMov64(uint32_t imm, RegisterN rdest)
    {
        InstrBuffer instr;
        instr = {
            MakeREX(R_NONE, rdest, false),
            0xC7,
            MakeModRM(false, true, imm, rdest),
            /* immediate data */
            (uint8_t)(imm & 0xFF),
            (uint8_t)((imm >> 8)  & 0xFF),
            (uint8_t)((imm >> 16) & 0xFF),
            (uint8_t)((imm >> 24) & 0xFF),
        };
        return instr;
    }
    InstrBuffer BuildJmp64(uint8_t near_ip)
    {
        InstrBuffer instr;
        instr = {
            0xEB,
            near_ip
        };
        return instr;
    }
    InstrBuffer BuildAdd64(RegisterN reg, int64_t imm)
    {
        InstrBuffer instr;
        instr = {
            MakeREX(R_NONE, reg, false),
            0x83,
            MakeModRM(true, true, imm, reg)
        };
        instr.push_back(imm);
        return instr;
    }

    InstrBuffer BuildPush64(RegisterN reg)
    {
        InstrBuffer instr = { };
        if (RegIsExtended(reg))
            instr.push_back(MakeREX(R_NONE, reg, false));
        instr.push_back(((0x05 << 4) | GetBaseReg(reg)));
        return instr;
    }

    InstrBuffer BuildPop64(RegisterN reg)
    {
        InstrBuffer instr = { };
        if (RegIsExtended(reg))
            instr.push_back(MakeREX(R_NONE, reg, false));
        instr.push_back(((0x05 << 4) | 0x08 | (GetBaseReg(reg) & 0x07)));
        return instr;
    }

    InstrBuffer StartFunction()
    {
        InstrBuffer instr = {
            BuildPush64(R_RBP),
            BuildMov64(R_RSP, R_RBP)
        };
        return instr;
    }

    InstrBuffer EndFunction()
    {
        InstrBuffer instr = {
            BuildPop64(R_RBP),
            BuildRet()
        };
        return instr;
    }

    InstrBuffer BuildCall64(uint32_t near_offset) {
        InstrBuffer instr = {
            0xE8,
            /* immediate data */
            (uint8_t)(near_offset & 0xFF),
            (uint8_t)((near_offset >> 8)  & 0xFF),
            (uint8_t)((near_offset >> 16) & 0xFF),
            (uint8_t)((near_offset >> 24) & 0xFF),
        };
        return instr;
    }

    InstrBuffer BuildRet() { return { 0xC3 }; }
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