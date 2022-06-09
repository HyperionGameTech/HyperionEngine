#ifndef BUILDABLE_HPP
#define BUILDABLE_HPP

#include <streambuf>
#include <vector>
#include <cstdint>

namespace hyperion::compiler {

using byte = uint8_t;
using Buffer = std::basic_streambuf<byte>;
using LabelPosition = uint32_t;

using Opcode = uint8_t;
using RegIndex = uint8_t;
using LabelId = size_t;

struct LabelInfo {
    LabelPosition position;
};

struct BuildParams {
    size_t block_offset = 0;
    size_t local_offset = 0;
    std::vector<LabelInfo> labels;
};

struct Buildable {
    virtual ~Buildable() = default;
};

} // namespace hyperion::compiler

#endif
