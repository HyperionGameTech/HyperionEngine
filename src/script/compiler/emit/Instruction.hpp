#ifndef INSTRUCTION_HPP
#define INSTRUCTION_HPP

#include <script/compiler/emit/NamesPair.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

#include <script/compiler/emit/Buildable.hpp>

#include <vector>
#include <ostream>
#include <cstring>
#include <cstdint>
#include <iostream>

namespace hyperion {
namespace compiler {

struct Instruction : public Buildable {
    Opcode opcode;

    virtual ~Instruction() = default;
};

struct LabelMarker final : public Buildable {
    LabelId id;

    LabelMarker(LabelId id)
        : id(id)
    {
    }
    virtual ~LabelMarker() = default;
};

struct Jump final : public Buildable {
    enum JumpClass {
        JMP,
        JE,
        JNE,
        JG,
        JGE,
    } jump_class;
    LabelId label_id;

    Jump() = default;
    Jump(JumpClass jump_class, LabelId label_id)
        : jump_class(jump_class),
          label_id(label_id)
    {
    }

    virtual ~Jump() = default;
};

struct Comparison final : public Buildable {
    enum ComparisonClass {
        CMP,
        CMPZ
    } comparison_class;

    RegIndex reg_lhs;
    RegIndex reg_rhs;

    Comparison() = default;

    Comparison(ComparisonClass comparison_class, RegIndex reg)
        : comparison_class(comparison_class),
          reg_lhs(reg)
    {
    }

    Comparison(ComparisonClass comparison_class, RegIndex reg_lhs, RegIndex reg_rhs)
        : comparison_class(comparison_class),
          reg_lhs(reg_lhs),
          reg_rhs(reg_rhs)
    {
    }

    virtual ~Comparison() = default;
};

struct FunctionCall : public Buildable {
    RegIndex reg;
    uint8_t nargs;

    FunctionCall() = default;
    FunctionCall(RegIndex reg, uint8_t nargs)
        : reg(reg),
          nargs(nargs)
    {
    }

    virtual ~FunctionCall() = default;
};

struct Return : public Buildable {
    Return() = default;
    virtual ~Return() = default;
};

struct StoreLocal : public Buildable {
    RegIndex reg;

    StoreLocal() = default;
    StoreLocal(RegIndex reg)
        : reg(reg)
    {
    }
    virtual ~StoreLocal() = default;
};

struct PopLocal : public Buildable {
    size_t amt;

    PopLocal() = default;
    PopLocal(size_t amt)
        : amt(amt)
    {
    }
    virtual ~PopLocal() = default;
};

struct LoadRef : public Buildable {
    RegIndex dst;
    RegIndex src;

    LoadRef() = default;
    LoadRef(RegIndex dst, RegIndex src)
        : dst(dst),
          src(src)
    {
    }

    virtual ~LoadRef() = default;
};

struct LoadDeref : public Buildable {
    RegIndex dst;
    RegIndex src;

    LoadDeref() = default;
    LoadDeref(RegIndex dst, RegIndex src)
        : dst(dst),
          src(src)
    {
    }

    virtual ~LoadDeref() = default;
};

struct ConstI32 : public Buildable {
    RegIndex reg;
    int32_t value;

    ConstI32() = default;
    ConstI32(RegIndex reg, int32_t value)
        : reg(reg),
          value(value)
    {
    }

    virtual ~ConstI32() = default;
};

struct ConstI64 : public Buildable {
    RegIndex reg;
    int64_t value;

    ConstI64() = default;
    ConstI64(RegIndex reg, int64_t value)
        : reg(reg),
          value(value)
    {
    }

    virtual ~ConstI64() = default;
};

struct ConstF32 : public Buildable {
    RegIndex reg;
    float value;

    ConstF32() = default;
    ConstF32(RegIndex reg, float value)
        : reg(reg),
          value(value)
    {
    }

    virtual ~ConstF32() = default;
};

struct ConstF64 : public Buildable {
    RegIndex reg;
    double value;

    ConstF64() = default;
    ConstF64(RegIndex reg, double value)
        : reg(reg),
          value(value)
    {
    }

    virtual ~ConstF64() = default;
};

struct ConstBool : public Buildable {
    RegIndex reg;
    bool value;

    ConstBool() = default;
    ConstBool(RegIndex reg, bool value)
        : reg(reg),
          value(value)
    {
    }

    virtual ~ConstBool() = default;
};

struct ConstNull : public Buildable {
    RegIndex reg;

    ConstNull() = default;
    ConstNull(RegIndex reg)
        : reg(reg)
    {
    }

    virtual ~ConstNull() = default;
};

struct BuildableTryCatch final : public Buildable {
    LabelId catch_label_id;
};

struct BuildableFunction final : public Buildable {
    RegIndex reg;
    LabelId label_id;
    uint8_t nargs;
    uint8_t flags;
};

struct BuildableType final : public Buildable {
    RegIndex reg;
    std::string name;
    std::vector<std::string> members;

    virtual ~BuildableType() = default;
};

struct BuildableString final : public Buildable {
    RegIndex reg;
    std::string value;

    virtual ~BuildableString() = default;
};

struct BinOp final : public Instruction {
    RegIndex reg_lhs;
    RegIndex reg_rhs;
    RegIndex reg_dst;
};

template <class...Args>
struct RawOperation final : public Instruction {
    std::vector<char> data;

    RawOperation() = default;
    RawOperation(const RawOperation &other)
        : data(other.data)
    {
    }

    void Accept(const char *str)
    {
        // do not copy NUL byte
        size_t length = std::strlen(str);
        data.insert(data.end(), str, str + length);
    }

    template <typename T>
    void Accept(const std::vector<T> &ts)
    {
        for (const T &t : ts) {
            this->Accept(t);
        }
    }

    template <typename T>
    void Accept(const T &t)
    {
        char bytes[sizeof(t)];
        std::memcpy(&bytes[0], &t, sizeof(t));
        data.insert(data.end(), &bytes[0], &bytes[0] + sizeof(t));
    }
};

template <class T, class... Ts>
struct RawOperation<T, Ts...> : RawOperation<Ts...> {
    RawOperation(T t, Ts...ts) : RawOperation<Ts...>(ts...)
        { this->Accept(t); }
};

} // namespace compiler
} // namespace hyperion

#endif
