#ifndef INSTRUCTION_HPP
#define INSTRUCTION_HPP

#include <script/compiler/emit/NamesPair.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

#include <script/compiler/emit/Buildable.hpp>

#include <vector>
#include <ostream>
#include <cstring>
#include <cstdint>
#include <iostream>

namespace hyperion::compiler {

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

struct ConstU32 : public Buildable {
    RegIndex reg;
    uint32_t value;

    ConstU32() = default;
    ConstU32(RegIndex reg, uint32_t value)
        : reg(reg),
          value(value)
    {
    }

    virtual ~ConstU32() = default;
};

struct ConstU64 : public Buildable {
    RegIndex reg;
    uint64_t value;

    ConstU64() = default;
    ConstU64(RegIndex reg, uint64_t value)
        : reg(reg),
          value(value)
    {
    }

    virtual ~ConstU64() = default;
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

    virtual ~BinOp() = default;
};

struct Comment final : public Instruction {
    std::string value;

    Comment() = default;
    Comment(const std::string &value) : value(value) {}
    Comment(const Comment &other) = default;
    virtual ~Comment() = default;
};

struct SymbolExport final : public Instruction {
    RegIndex reg;
    std::string name;

    SymbolExport() = default;
    SymbolExport(RegIndex reg, const std::string &name) : reg(reg), name(name) {}
    SymbolExport(const SymbolExport &other) = default;
    virtual ~SymbolExport() = default;
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

} // namespace hyperion::compiler


/*template <class...Ts>
struct Instruction {
public:
    Instruction()
    {
    }

    Instruction(const Instruction &other)
        : m_data(other.m_data)
    {
    }

    bool Empty() const
        { return !m_data.empty() && !m_data.back().empty(); }
    char GetOpcode() const
        { return m_data.back().back(); }

    std::vector<std::vector<char>> m_data;

protected:
    void Accept(NamesPair_t name)
    {
        std::vector<char> operand;

        char header[sizeof(name.first)];
        std::memcpy(&header[0], &name.first, sizeof(name.first));

        for (size_t j = 0; j < sizeof(name.first); j++) {
            operand.push_back(header[j]);
        }
        
        for (size_t j = 0; j < name.second.size(); j++) {
            operand.push_back(name.second[j]);
        }
        
        m_data.push_back(operand);
    }

    void Accept(std::vector<NamesPair_t> names)
    {
        std::vector<char> operand;

        for (size_t i = 0; i < names.size(); i++) {
            char header[sizeof(names[i].first)];
            std::memcpy(&header[0], &names[i].first, sizeof(names[i].first));

            for (size_t j = 0; j < sizeof(names[i].first); j++) {
                operand.push_back(header[j]);
            }
            for (size_t j = 0; j < names[i].second.size(); j++) {
                operand.push_back(names[i].second[j]);
            }
        }
        
        m_data.push_back(operand);
    }

    void Accept(const char *str)
    {
        // do not copy NUL byte
        size_t length = std::strlen(str);
        std::vector<char> operand;
        if (length) {
            operand.resize(length);
            std::memcpy(&operand[0], str, length);
        }
        m_data.push_back(operand);
    }

    template <typename T>
    void Accept(std::vector<T> ts)
    {
        std::vector<char> operand;
        operand.resize(sizeof(T) * ts.size());
        std::memcpy(&operand[0], &ts[0], sizeof(T) * ts.size());
        m_data.push_back(operand);
    }

    template <typename T>
    void Accept(T t)
    {
        std::vector<char> operand;
        operand.resize(sizeof(t));
        std::memcpy(&operand[0], &t, sizeof(t));
        m_data.push_back(operand);
    }

private:
    size_t pos = 0;
};

template <class T, class... Ts>
struct Instruction<T, Ts...> : Instruction<Ts...> {
    Instruction(T t, Ts...ts) : Instruction<Ts...>(ts...)
        { this->Accept(t); }
};*/

#endif
