#ifndef BUILDABLE_HPP
#define BUILDABLE_HPP

#include <core/lib/DynArray.hpp>
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
    SizeType block_offset = 0;
    SizeType local_offset = 0;
    Array<LabelInfo> labels;
};

struct Buildable
{
    virtual ~Buildable() = default;
};

} // namespace hyperion::compiler

#endif
