#ifndef BUILDABLE_HPP
#define BUILDABLE_HPP

#include <Types.hpp>

#include <streambuf>
#include <vector>
#include <cstdint>

namespace hyperion::compiler {

using byte = UByte;
using LabelPosition = UInt32;

using Opcode = UInt8;
using RegIndex = UInt8;
using LabelId = SizeType;

struct LabelInfo
{
    LabelPosition position;
};

struct BuildParams
{
    size_t block_offset = 0;
    size_t local_offset = 0;
    std::vector<LabelInfo> labels;
};

struct Buildable
{
    virtual ~Buildable() = default;
};

} // namespace hyperion::compiler

#endif
