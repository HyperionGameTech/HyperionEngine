/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GpuImage.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/dx12/DX12CommandBuffer.hpp>
#include <Rendering/dx12/DX12GpuBuffer.hpp>

// Fwd declaration
namespace D3D12MA { class Allocation; }

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GpuImage final : public GpuImageBase
{
    HYP_OBJECT_BODY(DX12GpuImage);

public:
    explicit DX12GpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE);
    ~DX12GpuImage() override;

    HYP_FORCE_INLINE ID3D12Resource* GetResource() const
    {
        return m_resource.Get();
    }

    HYP_FORCE_INLINE D3D12MA::Allocation* GetAllocation() const
    {
        return m_allocation.Get();
    }

    bool IsCreated() const override;
    bool IsOwned() const override;

    RendererResult Create() override;
    RendererResult Create(ResourceState initialState) override;

    RendererResult Resize(const Vec3u& extent) override;

    ResourceState GetSubResourceState(const ImageSubResource& subResource) const;
    void SetSubResourceState(const ImageSubResource& subResource, ResourceState newState);

    void ResetToAttachmentState()
    {
        const ResourceState attachmentState = m_textureDesc.IsDepthStencil()
            ? RS_DEPTH_STENCIL
            : RS_RENDER_TARGET;
        m_resourceState = attachmentState;
        m_stencilState = attachmentState;
        m_subResourceStates.Clear();
    }

    void InsertBarrier(
        DX12CommandBuffer* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType,
        bool onlyDepth = false,
        bool onlyStencil = false) override;

    void InsertBarrier(
        DX12CommandBuffer* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType,
        bool onlyDepth = false,
        bool onlyStencil = false) override;

    /*! \brief Inserts a UAV barrier to ensure all UAV writes complete before subsequent reads.
     *  Required in DX12 when reading from a subresource that was previously written as a UAV.
     *  \param commandBuffer The command buffer to insert the barrier into. */
    void InsertUAVBarrier(CommandBuffer* commandBuffer) override;

    void Blit(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuImage* srcImage) override;

    void Blit(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuImage* srcImage,
        const Rect<uint32>& srcRect,
        const Rect<uint32>& dstRect) override;

    void Blit(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuImage* srcImage,
        const Rect<uint32>& srcRect,
        const Rect<uint32>& dstRect,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) override;

    RendererResult GenerateMipmaps(DX12CommandBuffer* commandBuffer) override;

    void CopyFromBuffer(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuBuffer* srcBuffer,
        uint32 srcBufferOffset = 0,
        uint8 dstMipIndex = UINT8_MAX,
        uint16 dstArrayLayer = UINT16_MAX) const override;

    void CopyToBuffer(
        DX12CommandBuffer* commandBuffer,
        DX12GpuBuffer* dstBuffer,
        const ImageSubResource& subResource) const override;

    void CopyFrom(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuImage* srcImage,
        const Vec3u& srcOffset,
        const Vec3u& dstOffset,
        const Vec3u& extent,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) override;

    void Fill(
        DX12CommandBuffer* commandBuffer,
        float value,
        const ImageSubResource& subResource,
        const Vec3u& offset = Vec3u::Zero(),
        const Vec3u& extent = Vec3u::One()) override;

    DX12GpuImageViewRef MakeLayerImageView(uint32 layerIndex) const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;

    bool m_isHandleOwned = true;
    size_t m_size;
};

} // namespace Hyperion
