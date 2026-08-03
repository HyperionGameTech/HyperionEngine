#pragma once

#include <Lang/Compiler/Emit/NamesPair.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <Lang/Compiler/Emit/Buildable.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Reflection/ClassAttribute.hpp>

namespace Hyperion {
enum class MethodFlags : uint8;
enum class ClassFlags : uint8;
} // namespace Hyperion

namespace Hyperion {

struct Instruction : public Buildable
{
    Opcode opcode;
};

struct LabelMarker final : public Buildable
{
    LabelId id;

    LabelMarker(LabelId id)
        : id(id)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(LabelMarker)
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

    HYP_BUILDABLE_TYPE_IMPL(Jump)
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

    HYP_BUILDABLE_TYPE_IMPL(Comparison)
};

struct FunctionCall : public Buildable
{
    RegIndex reg;
    uint8 nargs;

    FunctionCall() = default;
    FunctionCall(RegIndex reg, uint8 nargs)
        : reg(reg),
          nargs(nargs)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(FunctionCall)
};

struct Return : public Buildable
{
    Return() = default;
    virtual ~Return() = default;

    HYP_BUILDABLE_TYPE_IMPL(Return)
};

struct StoreLocal : public Buildable
{
    RegIndex reg;

    StoreLocal() = default;
    StoreLocal(RegIndex reg)
        : reg(reg)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(StoreLocal)
};

struct PopLocal : public Buildable
{
    uint16 amt;

    PopLocal() = default;
    PopLocal(uint16 amt)
        : amt(amt)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(PopLocal)
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

    HYP_BUILDABLE_TYPE_IMPL(LoadRef)
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

    HYP_BUILDABLE_TYPE_IMPL(LoadDeref)
};

struct ConstI32 : public Buildable
{
    RegIndex reg;
    int32 value;

    ConstI32() = default;
    ConstI32(RegIndex reg, int32 value)
        : reg(reg),
          value(value)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstI32)
};

struct ConstI64 : public Buildable
{
    RegIndex reg;
    int64 value;

    ConstI64() = default;
    ConstI64(RegIndex reg, int64 value)
        : reg(reg),
          value(value)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstI64)
};

struct ConstU32 : public Buildable
{
    RegIndex reg;
    uint32 value;

    ConstU32() = default;
    ConstU32(RegIndex reg, uint32 value)
        : reg(reg),
          value(value)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstU32)
};

struct ConstU64 : public Buildable
{
    RegIndex reg;
    uint64 value;

    ConstU64() = default;
    ConstU64(RegIndex reg, uint64 value)
        : reg(reg),
          value(value)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstU64)
};

struct ConstF32 : public Buildable
{
    RegIndex reg;
    float32 value;

    ConstF32() = default;
    ConstF32(RegIndex reg, float value)
        : reg(reg),
          value(value)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstF32)
};

struct ConstF64 : public Buildable
{
    RegIndex reg;
    float64 value;

    ConstF64() = default;
    ConstF64(RegIndex reg, double value)
        : reg(reg),
          value(value)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstF64)
};

struct ConstBool : public Buildable
{
    RegIndex reg;
    uint8 value;

    ConstBool() = default;
    ConstBool(RegIndex reg, uint8 value)
        : reg(reg),
          value(value)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstBool)
};

struct ConstNull : public Buildable
{
    RegIndex reg;

    ConstNull() = default;
    ConstNull(RegIndex reg)
        : reg(reg)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(ConstNull)
};

struct LoadClass final : public Buildable
{
    RegIndex reg;
    StringHash nameHash;

    LoadClass() = default;
    LoadClass(RegIndex reg, StringHash nameHash)
        : reg(reg),
          nameHash(nameHash)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(LoadClass)
};

struct TryCatchInfo final : public Buildable
{
    LabelId catchLabelId;

    HYP_BUILDABLE_TYPE_IMPL(TryCatchInfo)
};

struct ScriptFunction final : public Buildable
{
    RegIndex reg = RegIndex(-1);
    LabelId labelId = LabelId(-1);
    uint8 nargs = 0;
    uint8 flags = 0;

    HYP_BUILDABLE_TYPE_IMPL(ScriptFunction)
};

struct ClassTable final : public Buildable
{
    struct MemberInfo
    {
        String name;
        TypeId typeId;
        ClassAttributeSet attrs;
        uint16 stackOffset = UINT16_MAX;
    };

    struct FieldInfo : MemberInfo
    {
        TypeId targetTypeId;
        uint32 offset = 0;
        uint32 size = 0;
    };

    struct MethodInfo : MemberInfo
    {
        TypeId targetTypeId;
        MethodFlags flags = (MethodFlags)0;
    };

    struct StaticFieldInfo : MemberInfo
    {
        TypeId targetTypeId;
        uint32 size = 0;
    };

    RegIndex reg;
    String name;
    Array<FieldInfo> fields;
    Array<MethodInfo> methods;
    Array<StaticFieldInfo> staticFields;
    ClassFlags flags = (ClassFlags)0;

    ~ClassTable() override = default;

    HYP_BUILDABLE_TYPE_IMPL(ClassTable)
};

struct ConstString final : public Buildable
{
    RegIndex reg;
    String value;

    ~ConstString() override = default;

    HYP_BUILDABLE_TYPE_IMPL(ConstString)
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
    ~Comment() override = default;

    HYP_BUILDABLE_TYPE_IMPL(Comment)
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
    ~SymbolExport() override = default;

    HYP_BUILDABLE_TYPE_IMPL(SymbolExport)
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
        CAST_STRING,
        CAST_DYNAMIC
    };

    Type type = CAST_U8;
    RegIndex regDst;
    RegIndex regSrc;
    uint64 typeNameHash = 0;

    CastOperation() = default;
    CastOperation(Type type, RegIndex regDst, RegIndex regSrc)
        : type(type),
          regDst(regDst),
          regSrc(regSrc)
    {
    }

    CastOperation(Type type, RegIndex regDst, RegIndex regSrc, uint64 typeNameHash)
        : type(type),
          regDst(regDst),
          regSrc(regSrc),
          typeNameHash(typeNameHash)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(CastOperation)
};

struct IsInstanceComp final : public Instruction
{
    RegIndex regDst;
    RegIndex regSrc;
    RegIndex regTypeRef;
    uint64 typeNameHash;

    IsInstanceComp() = default;
    IsInstanceComp(RegIndex regDst, RegIndex regSrc, RegIndex regTypeRef, uint64 typeNameHash)
        : regDst(regDst),
          regSrc(regSrc),
          regTypeRef(regTypeRef),
          typeNameHash(typeNameHash)
    {
    }

    HYP_BUILDABLE_TYPE_IMPL(IsInstanceComp)
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

    HYP_BUILDABLE_TYPE_IMPL(RawOperation)

    void Accept(const char* str)
    {
        // do not copy NUL byte
        size_t length = std::strlen(str);

        const size_t previousSize = data.Size();
        data.Resize(previousSize + length);
        Memory::Copy(data.Data() + previousSize, str, length);
    }

    template <typename T>
    void Accept(const Array<T>& ts)
    {
        for (const T& t : ts)
        {
            this->Accept(t);
        }
    }

    void Accept(ConstByteView byteView)
    {
        const size_t previousSize = data.Size();
        data.Resize(previousSize + byteView.Size());
        Memory::Copy(data.Data() + previousSize, byteView.Data(), byteView.Size());
    }

    template <typename T, typename = std::enable_if_t<std::is_fundamental_v<T> || std::is_enum_v<T>>>
    void Accept(T value)
    {
        const size_t previousSize = data.Size();
        data.Resize(previousSize + sizeof(T));
        Memory::Copy(data.Data() + previousSize, &value, sizeof(T));
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

} // namespace Hyperion
