#pragma once

#include "Backend/RxPipeline.hpp"
#include "Backend/RxShader.hpp"

#include <Core/FxHash.hpp>
#include <Core/FxRef.hpp>
#include <Core/FxStackArray.hpp>


class RxRenderPass;


class RxState
{
public:
    RxState();

    void BufferOffset(RxShaderType shader_type, uint32 buffer_offset);
    void Pipeline(RxPipeline* pipeline);
    void RenderPass(RxRenderPass* rp);

    void Apply(const RxCommandBuffer& cmd);

    void Reset();

    ~RxState();

private:
    RxPipeline* mpPipeline;
    RxRenderPass* mpRenderPass = nullptr;

    FxStackArray<RxShaderBindOptions, RxShaderUtil::scNumShaderTypes> mBindOptions;
};
