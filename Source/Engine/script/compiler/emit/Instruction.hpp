#pragma once

#include <script/compiler/emit/NamesPair.hpp>

#include <script/Instructions.hpp>
#include <Core/debug/Debug.hpp>

#include <script/compiler/emit/Buildable.hpp>

#include <Core/memory/Memory.hpp>
#include <Core/containers/String.hpp>
#include <Core/reflection/ClassAttribute.hpp>

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
};

struct PopLocal : public Buildable
{
    uint16 amt;

    PopLocal() = default;
    PopLocal(uint16 amt)
        : amt(amt)
    {
    }
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
};

struct ConstNull : public Buildable
{
    RegIndex reg;

    ConstNull() = default;
    ConstNull(RegIndex reg)
        : reg(reg)
    {
    }
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
};

struct TryCatchInfo final : public Buildable
{
    LabelId catchLabelId;
};

struct ScriptFunction final : public Buildable
{
    RegIndex reg = RegIndex(-1);
    LabelId labelId = LabelId(-1);
    uint8 nargs = 0;
    uint8 flags = 0;
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
};

struct ConstString final : public Buildable
{
    RegIndex reg;
    String value;

    ~ConstString() override = default;
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

    CastOperation() = default;
    CastOperation(Type type, RegIndex regDst, RegIndex regSrc)
        : type(type),
          regDst(regDst),
          regSrc(regSrc)
    {
    }
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
        const SizeType previousSize = data.Size();
        data.Resize(previousSize + byteView.Size());
        Memory::Copy(data.Data() + previousSize, byteView.Data(), byteView.Size());
    }

    template <typename T, typename = std::enable_if_t<std::is_fundamental_v<T> || std::is_enum_v<T>>>
    void Accept(T value)
    {
        const SizeType previousSize = data.Size();
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
