/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_DUMMY_DATA_HPP
#define HYPERION_DUMMY_DATA_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <math/MathUtil.hpp>

// #include <core/threading/Threads.hpp>

namespace hyperion {

using renderer::SampledImage2D;
using renderer::SampledImage3D;
using renderer::SampledImageCube;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::GPUBufferType;

class Engine;
class HYP_API PlaceholderData
{
public:
    PlaceholderData();
    PlaceholderData(const PlaceholderData &other)               = delete;
    PlaceholderData(PlaceholderData &&other)                    = delete;
    PlaceholderData &operator=(const PlaceholderData &other)    = delete;
    PlaceholderData &operator=(PlaceholderData &&other)         = delete;
    ~PlaceholderData();

#define HYP_DEF_DUMMY_DATA(type, getter, member) \
    public: \
        type##Ref &Get##getter() { return member; } \
        const type##Ref &Get##getter() const { return member; } \
    private: \
        type##Ref member

    HYP_DEF_DUMMY_DATA(Image, Image2D1x1R8, m_image_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView2D1x1R8, m_image_view_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(Image, Image2D1x1R8Storage, m_image_2d_1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView2D1x1R8Storage, m_image_view_2d_1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView3D1x1x1R8, m_image_view_3d_1x1x1_r8);
    HYP_DEF_DUMMY_DATA(Image, Image3D1x1x1R8, m_image_3d_1x1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView3D1x1x1R8Storage, m_image_view_3d_1x1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(Image, Image3D1x1x1R8Storage, m_image_3d_1x1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(Image, ImageCube1x1R8, m_image_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageViewCube1x1R8, m_image_view_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinear, m_sampler_linear);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinearMipmap, m_sampler_linear_mipmap);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerNearest, m_sampler_nearest);

#undef HYP_DEF_DUMMY_DATA

public:
    void Create();
    void Destroy();

    /*! \brief Get or create a buffer of at least the given size */
    GPUBufferRef GetOrCreateBuffer(Device *device, GPUBufferType buffer_type, SizeType required_size, bool exact_size = false)
    {
        // Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        if (!m_buffers.Contains(buffer_type)) {
            m_buffers.Set(buffer_type, { });
        }

        auto &buffer_container = m_buffers.At(buffer_type);

        // typename FlatMap<SizeType, GPUBufferWeakRef>::Iterator it;
        typename FlatMap<SizeType, GPUBufferRef>::Iterator it;
        
        if (exact_size) {
            it = buffer_container.Find(required_size);
        } else {
            it = buffer_container.LowerBound(required_size);
        }

        if (it != buffer_container.End()) {
            // if (auto ref = it->second.Lock(); ref.IsValid()) {
            //     return ref;
            // }

            if (it->second.IsValid()) {
                return it->second;
            }
        }

        if (!exact_size) {
            // use next power of 2 if exact size is not required,
            // this will allow this placeholder buffer to be re-used more.
            required_size = MathUtil::NextPowerOf2(required_size);
        }

        GPUBufferRef buffer = MakeRenderObject<GPUBuffer>(buffer_type);
        HYPERION_ASSERT_RESULT(buffer->Create(device, required_size));

        if (buffer->IsCPUAccessible()) {
            buffer->Memset(device, required_size, 0x0); // fill with zeros
        }

        const auto insert_result = buffer_container.Insert(required_size, buffer);
        AssertThrow(insert_result.second); // was inserted

        return buffer;
    }

private:
    // @TODO Replace with WeakRef version when GPUBufferRef is used in place of GPUBuffer * for all descriptor sets.
    // FlatMap<GPUBufferType, FlatMap<SizeType, GPUBufferWeakRef>> m_buffers;
    FlatMap<GPUBufferType, FlatMap<SizeType, GPUBufferRef>> m_buffers;
};

} // namespace hyperion

#endif
