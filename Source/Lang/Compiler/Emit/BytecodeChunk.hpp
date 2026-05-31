#pragma once

#include <Lang/Compiler/Emit/Instruction.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Optional.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

struct BytecodeChunk final : public Buildable
{
    Array<LabelId> labels;
    Array<UniquePtr<Buildable>> buildables;

    BytecodeChunk();
    BytecodeChunk(const BytecodeChunk& other) = delete;
    virtual ~BytecodeChunk() = default;

    void Append(UniquePtr<Buildable>&& buildable)
    {
        if (buildable != nullptr)
        {
            buildables.PushBack(std::move(buildable));
        }
    }

    void TakeOwnershipOfLabel(LabelId labelId)
    {
        labels.PushBack(labelId);
    }
};

} // namespace Hyperion
