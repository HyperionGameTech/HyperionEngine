/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMED_TEXTURE_DATA_HPP
#define HYPERION_STREAMED_TEXTURE_DATA_HPP

#include <streaming/StreamedData.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API StreamedTextureData final : public StreamedData
{
    HYP_OBJECT_BODY(StreamedTextureData);

    StreamedTextureData(StreamedDataState initial_state, TextureData texture_data, ResourceHandle &out_resource_handle);

public:
    StreamedTextureData();
    StreamedTextureData(const TextureData &texture_data, ResourceHandle &out_resource_handle);
    StreamedTextureData(TextureData &&texture_data, ResourceHandle &out_resource_handle);

    virtual ~StreamedTextureData() override = default;

    const TextureData &GetTextureData() const;
    void SetTextureData(TextureData &&texture_data);

    const TextureDesc &GetTextureDesc() const;
    void SetTextureDesc(const TextureDesc &texture_desc);

    HYP_FORCE_INLINE SizeType GetBufferSize() const
        { return m_buffer_size; }

    virtual HashCode GetDataHashCode() const override
        { return m_streamed_data ? m_streamed_data->GetDataHashCode() : HashCode(0); }

protected:
    virtual bool IsInMemory_Internal() const override;
    
    virtual void Load_Internal() const override;
    virtual void Unpage_Internal() override;
    
private:
    void LoadTextureData(const ByteBuffer &byte_buffer) const;

    RC<StreamedData>                m_streamed_data;
    
    mutable Optional<TextureData>   m_texture_data;
    mutable TextureDesc             m_texture_desc;
    mutable SizeType                m_buffer_size;
};

} // namespace hyperion

#endif