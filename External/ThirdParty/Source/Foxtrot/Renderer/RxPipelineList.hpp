#pragma once

#include "Backend/RxPipeline.hpp"

using RxPipelineId = uint32;

class RxPipelineList
{
public:
    RxPipelineList() {}

    void Create(uint32 max_pipelines) { Pipelines.InitCapacity(max_pipelines); }

    ~RxPipelineList() = default;

    FX_FORCE_INLINE RxPipelineId Insert(RxPipeline&& pipeline)
    {
        Pipelines.Insert(std::move(pipeline));
        // Return the last inserted pipeline's index
        return static_cast<RxPipelineId>(Pipelines.Size - 1);
    }

    FX_FORCE_INLINE RxPipeline* Get(RxPipelineId id)
    {
        if (id > Pipelines.Size) {
            FxLogError("Pipeline id out of range!");
            return nullptr;
        }

        return &Pipelines[id];
    }

public:
    FxSizedArray<RxPipeline> Pipelines;
};
