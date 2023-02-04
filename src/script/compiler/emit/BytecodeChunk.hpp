#ifndef BYTECODE_CHUNK_HPP
#define BYTECODE_CHUNK_HPP

#include <script/compiler/emit/Instruction.hpp>
#include <core/lib/DynArray.hpp>

#include <system/Debug.hpp>

#include <vector>
#include <memory>

namespace hyperion::compiler {

struct BytecodeChunk final : public Buildable
{
    Array<LabelInfo> labels;

    BytecodeChunk();
    BytecodeChunk(const BytecodeChunk &other) = delete;
    virtual ~BytecodeChunk() = default;

    void Append(std::unique_ptr<Buildable> buildable)
    {
        if (buildable != nullptr) {
            buildables.PushBack(std::move(buildable));
        }
    }

    LabelId NewLabel()
    {
        LabelId index = labels.Size();
        labels.PushBack(LabelInfo { });
        return index;
    }

    /*inline void MarkLabel(LabelId label_id)
    {
        AssertThrow(label_id < labels.Size());
        labels[label_id].position = chunk_size;
    }*/
    
    Array<std::unique_ptr<Buildable>> buildables;
};

} // namespace hyperion::compiler

#endif
