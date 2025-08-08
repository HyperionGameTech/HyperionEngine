/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/object/Handle.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderGpuImageView.hpp>
#include <rendering/RenderSampler.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderObject.hpp>

#include <core/math/MathUtil.hpp>

// #include <core/threading/Threads.hpp>

namespace hyperion {

class Texture;

template <TextureFormat Format>
HYP_API void FillPlaceholderBuffer_Tex2D(Vec2u dimensions, ByteBuffer& outBuffer);

template <TextureFormat Format>
HYP_API void FillPlaceholderBuffer_Cubemap(Vec2u dimensions, ByteBuffer& outBuffer);

class HYP_API PlaceholderData
{
public:
    PlaceholderData();
    PlaceholderData(const PlaceholderData& other) = delete;
    PlaceholderData(PlaceholderData&& other) = delete;
    PlaceholderData& operator=(const PlaceholderData& other) = delete;
    PlaceholderData& operator=(PlaceholderData&& other) = delete;
    ~PlaceholderData();

    Handle<Texture> defaultTexture2d;
    Handle<Texture> defaultTexture3d;
    Handle<Texture> defaultCubemap;
    Handle<Texture> defaultTexture2dArray;
    Handle<Texture> defaultCubemapArray;

#define HYP_DEF_DUMMY_DATA(type, getter, member) \
public:                                          \
    type##Ref& Get##getter()                     \
    {                                            \
        return member;                           \
    }                                            \
    const type##Ref& Get##getter() const         \
    {                                            \
        return member;                           \
    }                                            \
                                                 \
private:                                         \
    type##Ref member

    HYP_DEF_DUMMY_DATA(GpuImage, Image2D1x1R8, m_image2d1x1R8);
    HYP_DEF_DUMMY_DATA(GpuImageView, ImageView2D1x1R8, m_imageView2d1x1R8);
    HYP_DEF_DUMMY_DATA(GpuImage, Image2D1x1R8Storage, m_image2d1x1R8Storage);
    HYP_DEF_DUMMY_DATA(GpuImageView, ImageView2D1x1R8Storage, m_imageView2d1x1R8Storage);
    HYP_DEF_DUMMY_DATA(GpuImage, Image3D1x1x1R8, m_image3d1x1x1R8);
    HYP_DEF_DUMMY_DATA(GpuImageView, ImageView3D1x1x1R8, m_imageView3d1x1x1R8);
    HYP_DEF_DUMMY_DATA(GpuImage, Image3D1x1x1R8Storage, m_image3d1x1x1R8Storage);
    HYP_DEF_DUMMY_DATA(GpuImageView, ImageView3D1x1x1R8Storage, m_imageView3d1x1x1R8Storage);
    HYP_DEF_DUMMY_DATA(GpuImage, ImageCube1x1R8, m_imageCube1x1R8);
    HYP_DEF_DUMMY_DATA(GpuImageView, ImageViewCube1x1R8, m_imageViewCube1x1R8);
    HYP_DEF_DUMMY_DATA(GpuImage, Image2D1x1R8Array, m_image2d1x1R8Array);
    HYP_DEF_DUMMY_DATA(GpuImageView, ImageView2D1x1R8Array, m_imageView2d1x1R8Array);
    HYP_DEF_DUMMY_DATA(GpuImage, ImageCube1x1R8Array, m_imageCube1x1R8Array);
    HYP_DEF_DUMMY_DATA(GpuImageView, ImageViewCube1x1R8Array, m_imageViewCube1x1R8Array);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinear, m_samplerLinear);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinearMipmap, m_samplerLinearMipmap);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerNearest, m_samplerNearest);

#undef HYP_DEF_DUMMY_DATA

public:
    void Create();
    void Destroy();

    /*! \brief Get or create a buffer of at least the given size */
    GpuBufferRef GetOrCreateBuffer(GpuBufferType bufferType, SizeType requiredSize, bool exactSize = false)
    {
        // Threads::AssertOnThread(g_renderThread);

        if (!m_buffers.Contains(bufferType))
        {
            m_buffers.Set(bufferType, {});
        }

        auto& bufferContainer = m_buffers.At(bufferType);

        // typename FlatMap<SizeType, GpuBufferWeakRef>::Iterator it;
        typename FlatMap<SizeType, GpuBufferRef>::Iterator it;

        if (exactSize)
        {
            it = bufferContainer.Find(requiredSize);
        }
        else
        {
            it = bufferContainer.LowerBound(requiredSize);
        }

        if (it != bufferContainer.End())
        {
            // if (auto ref = it->second.Lock(); ref.IsValid()) {
            //     return ref;
            // }

            if (it->second.IsValid())
            {
                return it->second;
            }
        }

        if (!exactSize)
        {
            // use next power of 2 if exact size is not required,
            // this will allow this placeholder buffer to be re-used more.
            requiredSize = MathUtil::NextPowerOf2(requiredSize);
        }

        GpuBufferRef buffer = CreateGpuBuffer(bufferType, requiredSize);

        if (buffer->IsCpuAccessible())
        {
            buffer->Memset(requiredSize, 0); // fill with zeros
        }

        const auto insertResult = bufferContainer.Insert(requiredSize, buffer);
        AssertDebug(insertResult.second); // check was inserted

        return buffer;
    }

private:
    GpuBufferRef CreateGpuBuffer(GpuBufferType bufferType, SizeType size);

    FlatMap<GpuBufferType, FlatMap<SizeType, GpuBufferRef>> m_buffers;
};

} // namespace hyperion
