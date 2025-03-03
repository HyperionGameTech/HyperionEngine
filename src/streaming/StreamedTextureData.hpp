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

    StreamedTextureData(StreamedDataState initial_state, TextureData texture_data);

public:
    static RC<StreamedTextureData> FromTextureData(TextureData);
    
    StreamedTextureData();
    StreamedTextureData(const TextureData &texture_data);
    StreamedTextureData(TextureData &&texture_data);
    virtual ~StreamedTextureData() override = default;

    const TextureData &GetTextureData() const;

    HYP_FORCE_INLINE const TextureDesc &GetTextureDesc() const
        { return m_texture_desc; }

    HYP_FORCE_INLINE SizeType GetBufferSize() const
        { return m_buffer_size; }

    HYP_FORCE_INLINE StreamedDataRef<StreamedTextureData> AcquireRef()
        { return { RefCountedPtrFromThis().CastUnsafe<StreamedTextureData>() }; }

protected:
    virtual bool IsNull_Internal() const override;
    virtual bool IsInMemory_Internal() const override;
    
    virtual const ByteBuffer &Load_Internal() const override;
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