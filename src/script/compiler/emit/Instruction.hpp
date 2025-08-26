#pragma once

#include <script/compiler/emit/NamesPair.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <script/compiler/emit/Buildable.hpp>

#include <core/memory/Memory.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/TypeId.hpp>

#include <vector>
#include <ostream>
#include <cstring>
#include <cstdint>
#include <iostream>

namespace hyperion {
class HypField;
class HypMethod;
class HypConstant;
class HypProperty;
class IHypMember;
} // namespace hyperion

namespace hyperion::compiler {

struct Instruction : public Buildable
{
    Opcode opcode;

    virtual ~Instruction() = default;
};

struct LabelMarker final : public Buildable
{
    LabelId id;

    LabelMarker(LabelId id)
        : id(id)
    {
    }
    virtual ~LabelMarker() = default;
};

struct Jump final : public Buildable
{
    enum JumpClass
    {
        JMP,
        JE,
        JNE,
        JG,
        JGE,
    } jumpClass;
    LabelId labelId;

    Jump() = default;
    Jump(JumpClass jumpClass, LabelId labelId)
        : jumpClass(jumpClass),
          labelId(labelId)
    {
    }

    virtual ~Jump() = default;
};

struct Comparison final : public Buildable
{
    enum ComparisonClass
    {
        CMP,
        CMPZ
    } comparisonClass;

    RegIndex regLhs;
    RegIndex regRhs;

    Comparison() = default;

    Comparison(ComparisonClass comparisonClass, RegIndex reg)
        : comparisonClass(comparisonClass),
          regLhs(reg)
    {
    }

    Comparison(ComparisonClass comparisonClass, RegIndex regLhs, RegIndex regRhs)
        : comparisonClass(comparisonClass),
          regLhs(regLhs),
          regRhs(regRhs)
    {
    }

    virtual ~Comparison() = default;
};

struct FunctionCall : public Buildable
{
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

struct Return : public Buildable
{
    Return() = default;
    virtual ~Return() = default;
};

struct StoreLocal : public Buildable
{
    RegIndex reg;

    StoreLocal() = default;
    StoreLocal(RegIndex reg)
        : reg(reg)
    {
    }
    virtual ~StoreLocal() = default;
};

struct PopLocal : public Buildable
{
    uint16 amt;

    PopLocal() = default;
    PopLocal(uint16 amt)
        : amt(amt)
    {
    }
    virtual ~PopLocal() = default;
};

struct LoadRef : public Buildable
{
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

struct LoadDeref : public Buildable
{
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

struct ConstI32 : public Buildable
{
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

struct ConstI64 : public Buildable
{
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

struct ConstU32 : public Buildable
{
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

struct ConstU64 : public Buildable
{
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

struct ConstF32 : public Buildable
{
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

struct ConstF64 : public Buildable
{
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

struct ConstBool : public Buildable
{
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

struct ConstNull : public Buildable
{
    RegIndex reg;

    ConstNull() = default;
    ConstNull(RegIndex reg)
        : reg(reg)
    {
    }

    virtual ~ConstNull() = default;
};

struct LoadClass final : public Buildable
{
    RegIndex reg;
    uint64 nameHash;

    LoadClass() = default;
    LoadClass(RegIndex reg, uint64 nameHash)
        : reg(reg),
          nameHash(nameHash)
    {
    }

    virtual ~LoadClass() = default;
};

struct BuildableTryCatch final : public Buildable
{
    LabelId catchLabelId;
};

struct BuildableFunction final : public Buildable
{
    RegIndex reg;
    LabelId labelId;
    uint8_t nargs;
    uint8_t flags;
};

struct BuildableType final : public Buildable
{
    RegIndex reg;
    String name;
    Array<HypField*> fields;
    Array<HypMethod*> methods;
    Array<HypConstant*> constants;
    Array<HypProperty*> properties;

    virtual ~BuildableType();
};

struct BuildableString final : public Buildable
{
    RegIndex reg;
    String value;

    virtual ~BuildableString() = default;
};

struct BinOp final : public Instruction
{
    RegIndex regLhs;
    RegIndex regRhs;
    RegIndex regDst;

    virtual ~BinOp() = default;
};

struct Comment final : public Instruction
{
    String value;

    Comment() = default;
    Comment(const String& value)
        : value(value)
    {
    }
    Comment(const Comment& other) = default;
    virtual ~Comment() = default;
};

struct SymbolExport final : public Instruction
{
    RegIndex reg;
    String name;

    SymbolExport() = default;
    SymbolExport(RegIndex reg, const String& name)
        : reg(reg),
          name(name)
    {
    }
    SymbolExport(const SymbolExport& other) = default;
    virtual ~SymbolExport() = default;
};

struct CastOperation final : public Instruction
{
    enum Type
    {
        CAST_U8,
        CAST_U16,
        CAST_U32,
        CAST_U64,
        CAST_I8,
        CAST_I16,
        CAST_I32,
        CAST_I64,
        CAST_F32,
        CAST_F64,
        CAST_BOOL,
        CAST_DYNAMIC
    };

    Type type = CAST_U8;
    RegIndex regDst;
    RegIndex regSrc;

    CastOperation() = default;
    CastOperation(Type type, RegIndex regDst, RegIndex regSrc)
        : type(type),
          regDst(regDst),
          regSrc(regSrc)
    {
    }

    virtual ~CastOperation() = default;
};

template <class... Args>
struct RawOperation final : public Instruction
{
    Array<char> data;

    RawOperation() = default;
    RawOperation(const RawOperation& other)
        : data(other.data)
    {
    }

    void Accept(const char* str)
    {
        // do not copy NUL byte
        SizeType length = std::strlen(str);

        const SizeType previousSize = data.Size();
        data.Resize(previousSize + length);
        Memory::MemCpy(data.Data() + previousSize, str, length);
    }

    template <typename T>
    void Accept(const Array<T>& ts)
    {
        for (const T& t : ts)
        {
            this->Accept(t);
        }
    }

    template <typename T>
    void Accept(const T& t)
    {
        const SizeType previousSize = data.Size();
        data.Resize(previousSize + sizeof(T));
        Memory::MemCpy(data.Data() + previousSize, &t, sizeof(T));
    }
};

template <class T, class... Ts>
struct RawOperation<T, Ts...> : RawOperation<Ts...>
{
    RawOperation(T t, Ts... ts)
        : RawOperation<Ts...>(ts...)
    {
        this->Accept(t);
    }
};

} // namespace hyperion::compiler
