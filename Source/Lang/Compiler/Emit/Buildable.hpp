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

struct Buildable
{
    virtual ~Buildable() = default;
};

} // namespace Hyperion
