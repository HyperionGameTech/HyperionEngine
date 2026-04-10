#pragma once

#include "Backend/RxDescriptors.hpp"
#include "Backend/RxFramebuffer.hpp"
#include "Backend/RxImage.hpp"
#include "Backend/RxRenderPass.hpp"
#include "Backend/RxSampler.hpp"

#include <Core/FxSizedArray.hpp>

class RxRenderStage
{
    static constexpr uint32 scMaxInputAttachments = 6;
    static constexpr uint32 scMaxOutputTargets = 8;
    static constexpr uint32 scMaxInputBuffers = 2;

    struct InputTarget
    {
        uint32 BindIndex = 0;
        RxTarget* pTarget = nullptr;
        RxSampler* pSampler = nullptr;
        RxRawGpuBuffer* pBuffer = nullptr;

        uint32 BufferOffset = 0;
        uint32 BufferRange = 0;
    };

public:
    RxRenderStage() = default;

    void Create(const FxVec2u& size)
    {
        ClearValues.InitCapacity(scMaxOutputTargets);
        mSize = size;
    }

    void AddTarget(RxImageFormat format, const FxVec2u& size, VkImageUsageFlags usage, RxImageAspectFlag aspect);
    void AddTarget(const RxTarget& attachment);

    RxTargetList& GetTargets() { return mOutputTargets; }
    RxTarget* GetTarget(RxImageFormat format, int sub_index = 0);

    void MarkFinalStage();

    RxRenderPass& GetRenderPass() { return mRenderPass; }
    RxFramebuffer& GetFramebuffer() { return mFramebuffer; }

    /**
     * @brief Builds the Vulkan objects for the render stage. This is deferred until the user requests a renderpass or
     * attachment. This is to reduce unused render stages, allow changes before the stage is built, etc.
     */
    void BuildRenderStage();

    void Begin(RxCommandBuffer& cmd, RxPipeline& pipeline);

    void End() { mRenderPass.End(); }

    ~RxRenderStage() = default;

private:
    void MakeClearValues();
    void CreateFinalStageFramebuffers();

    void AddPresentTarget();

public:
    FxSizedArray<VkClearValue> ClearValues;

private:
    RxTargetList mOutputTargets;
    FxSizedArray<InputTarget> mInputTargets;

    RxFramebuffer mFramebuffer;
    RxRenderPass mRenderPass;
    FxVec2u mSize = FxVec2u::sZero;

    bool mbIsBuilt : 1 = false;


    bool mbIsFinalStage = false;
    FxSizedArray<RxFramebuffer> mFinalStageFramebuffers;
};
