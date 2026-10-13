/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

namespace Hyperion {

// Matches the argument layout of GPURenderPassEncoder.drawIndexedIndirect
struct IndirectDrawCommand
{
    uint32 IndexCountPerInstance;
    uint32 InstanceCount;
    uint32 StartIndexLocation;
    int32 BaseVertexLocation;
    uint32 StartInstanceLocation;
};

static_assert(sizeof(IndirectDrawCommand) == 20, "Verify size of struct in shader");

} // namespace Hyperion
