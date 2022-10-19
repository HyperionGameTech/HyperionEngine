#ifndef HYPERION_V2_DUMMY_DATA_H
#define HYPERION_V2_DUMMY_DATA_H

#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <math/MathUtil.hpp>

#include <Threads.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::TextureImage2D;
using renderer::TextureImage3D;
using renderer::TextureImageCube;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::UniformBuffer;
using renderer::Device;
using renderer::GPUBuffer;

class Engine;

class PlaceholderData
{
public:
    PlaceholderData();

#define HYP_DEF_DUMMY_DATA(type, getter, member) \
    public: \
        type &Get##getter() { return member; } \
        const type &Get##getter() const { return member; } \
    private: \
        type member

    HYP_DEF_DUMMY_DATA(TextureImage2D, Image2D1x1R8, m_image_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView2D1x1R8, m_image_view_2d_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView3D1x1x1R8, m_image_view_3d_1x1x1_r8);
    HYP_DEF_DUMMY_DATA(TextureImage3D, Image3D1x1x1R8, m_image_3d_1x1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageView3D1x1x1R8Storage, m_image_view_3d_1x1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(StorageImage, Image3D1x1x1R8Storage, m_image_3d_1x1x1_r8_storage);
    HYP_DEF_DUMMY_DATA(TextureImageCube, ImageCube1x1R8, m_image_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(ImageView, ImageViewCube1x1R8, m_image_view_cube_1x1_r8);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinear, m_sampler_linear);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerLinearMipmap, m_sampler_linear_mipmap);
    HYP_DEF_DUMMY_DATA(Sampler, SamplerNearest, m_sampler_nearest);

#undef HYP_DEF_DUMMY_DATA

public:
    void Create(Engine *engine);
    void Destroy(Engine *engine);

    /*! \brief Get or create a buffer of at least the given size */
    template <class T>
    T *GetOrCreateBuffer(Device *device, SizeType required_size)
    {
        static_assert(std::is_base_of_v<GPUBuffer, T>, "Must be a derived class of GPUBuffer");

        Threads::AssertOnThread(THREAD_RENDER);

        if (!m_buffers.Contains<T>()) {
            m_buffers.Set<T>({ });
        }

        auto &buffer_container = m_buffers.At<T>();

        auto it = buffer_container.LowerBound(required_size);

        if (it != buffer_container.End()) {
            return static_cast<T *>(it->second.get());
        }

        const auto required_size_pow2 = MathUtil::NextPowerOf2(required_size);

        auto buffer = std::make_unique<T>();
        HYPERION_ASSERT_RESULT(buffer->Create(device, required_size_pow2));
        buffer->Memset(device, required_size_pow2, 0x00); // fill with zeros

        const auto insert_result = buffer_container.Insert(required_size_pow2, std::move(buffer));
        AssertThrow(insert_result.second); // was inserted

        // TODO: GC of these buffers. they'll have to be reference counted?

        return static_cast<T *>(insert_result.first->second.get());
    }

private:
    TypeMap<FlatMap<SizeType, std::unique_ptr<GPUBuffer>>> m_buffers;
};

} // namespace hyperion::v2

#endif
