#pragma once

#include <script/compiler/emit/Instruction.hpp>

#include <Core/containers/Array.hpp>

#include <Core/utilities/Optional.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/name/Name.hpp>

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
