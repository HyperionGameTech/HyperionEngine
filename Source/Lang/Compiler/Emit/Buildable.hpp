#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/SortedArray.hpp>
#include <Core/Name/Name.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

using byte = ubyte;
using LabelPosition = uint32;

using Opcode = uint8;
using RegIndex = uint8;
using LabelId = size_t;

struct LabelInfo
{
    LabelId labelId = LabelId(-1);
    LabelPosition position = LabelPosition(-1);
    Name name = HYP_NAME(LabelNameNotSet);

    bool operator==(const LabelInfo& other) const
    {
        return labelId == other.labelId
            && position == other.position
            && name == other.name;
    }

    bool operator<(const LabelInfo& other) const
    {
        return labelId < other.labelId;
    }
};

struct BuildParams
{
    size_t blockOffset = 0;
    size_t localOffset = 0;
    SortedArray<LabelInfo> labels;
};

enum class BuildableType
{
    BytecodeChunk,
    LabelMarker,
    Jump,
    Comparison,
    FunctionCall,
    Return,
    StoreLocal,
    PopLocal,
    LoadRef,
    LoadDeref,
    ConstI32,
    ConstI64,
    ConstU32,
    ConstU64,
    ConstF32,
    ConstF64,
    ConstBool,
    ConstNull,
    LoadClass,
    TryCatchInfo,
    ScriptFunction,
    ClassTable,
    ConstString,
    StorageOperation,
    Comment,
    SymbolExport,
    CastOperation,
    IsInstanceComp,
    RawOperation,

    Unknown
};

struct Buildable
{
    virtual ~Buildable() = default;
    virtual BuildableType GetBuildableType() const = 0;
};

#define HYP_BUILDABLE_TYPE_IMPL(Type) \
    BuildableType GetBuildableType() const override { return BuildableType::Type; }

} // namespace Hyperion
