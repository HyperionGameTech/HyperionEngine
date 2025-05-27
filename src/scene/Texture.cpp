/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Texture.hpp>

#include <rendering/RenderTexture.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

class Texture;
class TextureMipmapRenderer;

const FixedArray<Pair<Vec3f, Vec3f>, 6> Texture::cubemap_directions = {
    Pair<Vec3f, Vec3f> { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { -1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 1, 0 }, Vec3f { 0, 0, -1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, -1, 0 }, Vec3f { 0, 0, 1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, 1 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, -1 }, Vec3f { 0, 1, 0 } }
};

#pragma region Texture

Texture::Texture()
    : Texture(TextureDesc {
          ImageType::TEXTURE_TYPE_2D,
          InternalFormat::RGBA8,
          Vec3u { 1, 1, 1 },
          FilterMode::TEXTURE_FILTER_NEAREST,
          FilterMode::TEXTURE_FILTER_NEAREST,
          WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE })
{
}

Texture::Texture(const TextureDesc& texture_desc)
    : m_render_resource(nullptr),
      m_texture_desc(texture_desc),
      m_streamed_texture_data(MakeRefCountedPtr<StreamedTextureData>(TextureData { texture_desc }, m_streamed_texture_data_resource_handle))
{
}

Texture::Texture(const TextureData& texture_data)
    : m_render_resource(nullptr),
      m_texture_desc(texture_data.desc),
      m_streamed_texture_data(MakeRefCountedPtr<StreamedTextureData>(texture_data, m_streamed_texture_data_resource_handle))
{
}

Texture::Texture(const RC<StreamedTextureData>& streamed_texture_data)
    : m_render_resource(nullptr),
      m_texture_desc(streamed_texture_data != nullptr ? streamed_texture_data->GetTextureDesc() : TextureDesc {}),
      m_streamed_texture_data(streamed_texture_data)
{
    if (m_streamed_texture_data)
    {
        m_streamed_texture_data_resource_handle = ResourceHandle(*m_streamed_texture_data);
    }
}

Texture::~Texture()
{
    m_persistent_render_resource_handle.Reset();

    if (m_render_resource)
    {
        FreeResource(m_render_resource);
        m_render_resource = nullptr;
    }

    m_streamed_texture_data_resource_handle.Reset();

    if (m_streamed_texture_data)
    {
        m_streamed_texture_data->WaitForFinalization();
        m_streamed_texture_data.Reset();
    }
}

void Texture::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            m_persistent_render_resource_handle.Reset();

            if (m_render_resource)
            {
                FreeResource(m_render_resource);
                m_render_resource = nullptr;
            }

            m_streamed_texture_data_resource_handle.Reset();

            if (m_streamed_texture_data)
            {
                m_streamed_texture_data->WaitForFinalization();
                m_streamed_texture_data.Reset();
            }
        }));

    m_render_resource = AllocateResource<TextureRenderResource>(this);

    m_streamed_texture_data_resource_handle.Reset();

    SetReady(true);
}

void Texture::SetStreamedTextureData(const RC<StreamedTextureData>& streamed_texture_data)
{
    Mutex::Guard guard(m_readback_mutex);

    if (m_streamed_texture_data == streamed_texture_data)
    {
        return;
    }

    m_streamed_texture_data_resource_handle.Reset();

    m_streamed_texture_data = streamed_texture_data;

    if (m_streamed_texture_data && IsInitCalled())
    {
        m_streamed_texture_data_resource_handle = ResourceHandle(*m_streamed_texture_data);
    }

    // @TODO Reupload texture data if already initialized
}

void Texture::SetTextureDesc(const TextureDesc& texture_desc)
{
    Mutex::Guard guard(m_readback_mutex);

    if (m_texture_desc == texture_desc)
    {
        return;
    }

    m_texture_desc = texture_desc;

    // Update streamed data
    if (m_streamed_texture_data)
    {
        bool has_resource_handle = bool(m_streamed_texture_data_resource_handle);

        ByteBuffer texture_data_buffer;

        if (!has_resource_handle)
        {
            m_streamed_texture_data_resource_handle = ResourceHandle(*m_streamed_texture_data);
        }

        texture_data_buffer = m_streamed_texture_data->GetTextureData().buffer;

        m_streamed_texture_data_resource_handle.Reset();
        m_streamed_texture_data->WaitForFinalization();

        // Create a new StreamedTextureData, with the newly set TextureDesc.
        m_streamed_texture_data = MakeRefCountedPtr<StreamedTextureData>(TextureData {
                                                                             m_texture_desc,
                                                                             std::move(texture_data_buffer) },
            m_streamed_texture_data_resource_handle);

        // If we didn't need it before assume we don't need it now.
        if (!has_resource_handle)
        {
            m_streamed_texture_data_resource_handle.Reset();
        }
    }

    // @TODO Reupload texture data if already initialized
    if (IsInitCalled())
    {
        // @TODO Reupload texture data
    }
}

void Texture::GenerateMipmaps()
{
    AssertReady();

    m_render_resource->Claim();
    m_render_resource->RenderMipmaps();
    m_render_resource->Unclaim();
}

void Texture::Readback()
{
    Mutex::Guard guard(m_readback_mutex);

    Readback_Internal();
}

void Texture::Readback_Internal()
{
    AssertReady();

    ByteBuffer result_byte_buffer;
    m_render_resource->Readback(result_byte_buffer);

    const SizeType expected = m_texture_desc.GetByteSize();
    const SizeType real = result_byte_buffer.Size();

    AssertThrowMsg(expected == real, "Failed to readback texture: expected size: %llu, got %llu", expected, real);

    m_streamed_texture_data_resource_handle.Reset();

    m_streamed_texture_data = MakeRefCountedPtr<StreamedTextureData>(TextureData {
                                                                         m_texture_desc,
                                                                         std::move(result_byte_buffer) },
        m_streamed_texture_data_resource_handle);
}

void Texture::Resize(const Vec3u& extent)
{
    if (m_texture_desc.extent == extent)
    {
        return;
    }

    m_texture_desc.extent = extent;

    if (m_render_resource)
    {
        m_render_resource->Resize(extent);
    }
}

Vec4f Texture::Sample(Vec3f uvw, uint32 face_index)
{
    if (!IsReady())
    {
        HYP_LOG(Texture, Warning, "Texture is not ready, cannot sample");

        return Vec4f::Zero();
    }

    if (face_index >= NumFaces())
    {
        HYP_LOG(Texture, Warning, "Face index out of bounds: {} >= {}", face_index, NumFaces());

        return Vec4f::Zero();
    }

    // keep reference alive in case m_streamed_texture_data changes outside of the mutex lock.
    RC<StreamedTextureData> streamed_texture_data;
    TResourceHandle<StreamedTextureData> resource_handle;

    {
        Mutex::Guard guard(m_readback_mutex);

        if (!m_streamed_texture_data)
        {
            HYP_LOG(Texture, Warning, "Texture does not have streamed data present, attempting readback...");

            Readback_Internal();

            if (!m_streamed_texture_data)
            {
                HYP_LOG(Texture, Warning, "Texture readback failed. Sample will return zero.");

                return Vec4f::Zero();
            }
        }

        resource_handle = TResourceHandle<StreamedTextureData>(*m_streamed_texture_data);

        if (resource_handle->GetTextureData().buffer.Size() == 0)
        {
            HYP_LOG(Texture, Warning, "Texture buffer is empty; forcing readback.");

            resource_handle.Reset();

            Readback_Internal();

            if (!m_streamed_texture_data)
            {
                HYP_LOG(Texture, Warning, "Texture readback failed. Sample will return zero.");

                return Vec4f::Zero();
            }

            resource_handle = TResourceHandle<StreamedTextureData>(*m_streamed_texture_data);

            if (resource_handle->GetTextureData().buffer.Size() == 0)
            {
                HYP_LOG(Texture, Warning, "Texture buffer is still empty after readback; sample will return zero.");

                return Vec4f::Zero();
            }
        }

        streamed_texture_data = m_streamed_texture_data;
    }

    const TextureData* texture_data = &resource_handle->GetTextureData();

    const Vec3u coord = {
        uint32(uvw.x * float(texture_data->desc.extent.x - 1) + 0.5f),
        uint32(uvw.y * float(texture_data->desc.extent.y - 1) + 0.5f),
        uint32(uvw.z * float(texture_data->desc.extent.z - 1) + 0.5f)
    };

    const uint32 bytes_per_pixel = renderer::NumBytes(texture_data->desc.format);

    if (bytes_per_pixel != 1)
    {
        HYP_LOG(Texture, Warning, "Unsupported bytes per pixel to use with Sample(): {}", bytes_per_pixel);

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    const uint32 num_components = renderer::NumComponents(texture_data->desc.format);

    const uint32 index = face_index * (texture_data->desc.extent.x * texture_data->desc.extent.y * texture_data->desc.extent.z * bytes_per_pixel * num_components)
        + coord.z * (texture_data->desc.extent.x * texture_data->desc.extent.y * bytes_per_pixel * num_components)
        + coord.y * (texture_data->desc.extent.x * bytes_per_pixel * num_components)
        + coord.x * bytes_per_pixel * num_components;

    if (index >= texture_data->buffer.Size())
    {
        HYP_LOG(Texture, Warning, "Index out of bounds, index: {}, buffer size: {}, coord: {}, dimensions: {}, num faces: {}", index, texture_data->buffer.Size(),
            coord, texture_data->desc.extent, NumFaces());

        return Vec4f::Zero();
    }

    const ubyte* data = texture_data->buffer.Data() + index;

    switch (num_components)
    {
    case 1:
        return Vec4f(float(data[0]) / 255.0f);
    case 2:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, 0.0f, 1.0f);
    case 3:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, 1.0f);
    case 4:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, float(data[3]) / 255.0f);
    default: // should never happen
        HYP_LOG(Texture, Error, "Unsupported number of components: {}", num_components);

        return Vec4f::Zero();
    }
}

Vec4f Texture::Sample2D(Vec2f uv)
{
    if (GetType() != ImageType::TEXTURE_TYPE_2D)
    {
        HYP_LOG(Texture, Warning, "Unsupported texture type to use with Sample2D(): {}", GetType());

        return Vec4f::Zero();
    }

    return Sample(Vec3f { uv.x, uv.y, 0.0f }, 0);
}

/// https://www.gamedev.net/forums/topic/687535-implementing-a-cube-map-lookup-function/5337472/
Vec4f Texture::SampleCube(Vec3f direction)
{
    if (GetType() != ImageType::TEXTURE_TYPE_CUBEMAP)
    {
        HYP_LOG(Texture, Warning, "Unsupported texture type to use with SampleCube(): {}", GetType());

        return Vec4f::Zero();
    }

    Vec3f abs_dir = MathUtil::Abs(direction);
    uint32 face_index = 0;

    float mag;
    Vec2f uv;

    if (abs_dir.z >= abs_dir.x && abs_dir.z >= abs_dir.y)
    {
        mag = abs_dir.z;

        if (direction.z < 0.0f)
        {
            face_index = 5;
            uv = Vec2f(-direction.x, -direction.y);
        }
        else
        {
            face_index = 4;
            uv = Vec2f(direction.x, -direction.y);
        }
    }
    else if (abs_dir.y >= abs_dir.x)
    {
        mag = abs_dir.y;

        if (direction.y < 0.0f)
        {
            face_index = 3;
            uv = Vec2f(direction.x, -direction.z);
        }
        else
        {
            face_index = 2;
            uv = Vec2f(direction.x, direction.z);
        }
    }
    else
    {
        mag = abs_dir.x;

        if (direction.x < 0.0f)
        {
            face_index = 1;
            uv = Vec2f(direction.z, -direction.y);
        }
        else
        {
            face_index = 0;
            uv = Vec2f(-direction.z, -direction.y);
        }
    }

    return Sample(Vec3f { uv / mag * 0.5f + 0.5f, 0.0f }, face_index);
}

void Texture::SetPersistentRenderResourceEnabled(bool enabled)
{
    AssertReady();

    if (enabled)
    {
        if (!m_persistent_render_resource_handle)
        {
            m_persistent_render_resource_handle = ResourceHandle(*m_render_resource);
        }
    }
    else
    {
        m_persistent_render_resource_handle.Reset();
    }
}

#pragma endregion Texture

} // namespace hyperion
