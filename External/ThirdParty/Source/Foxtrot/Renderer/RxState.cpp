#include "RxState.hpp"

#include "Backend/RxRenderPass.hpp"
#include "Backend/RxShader.hpp"
#include "RxPipelineBuilder.hpp"

#include <Core/FxSlice.hpp>

RxState::RxState() {}

void RxState::Pipeline(RxPipeline* pipeline) { mpPipeline = pipeline; }

void RxState::BufferOffset(RxShaderType shader_type, uint32 buffer_offset)
{
    RxShaderBindOptions& dyn = mBindOptions[static_cast<uint32>(shader_type)];
    dyn.bUseOffset = true;
    dyn.BufferOffset = buffer_offset;
}

void RxState::RenderPass(RxRenderPass* rp) { mpRenderPass = rp; }

void RxState::Apply(const RxCommandBuffer& cmd)
{
    // mpPipeline->Bind(cmd);

    mpPipeline->VertexShader->Bind(cmd, *mpPipeline, mBindOptions[static_cast<uint32>(RxShaderType::eVertex)]);
    mpPipeline->FragmentShader->Bind(cmd, *mpPipeline, mBindOptions[static_cast<uint32>(RxShaderType::eFragment)]);
}

void RxState::Reset()
{
    memset(mBindOptions.pData, 0, mBindOptions.GetSizeInBytes());
    mpPipeline = nullptr;
    mpRenderPass = nullptr;
}


RxState::~RxState() {}
