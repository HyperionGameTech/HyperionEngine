/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/object/Handle.hpp>

#include <scene/Entity.hpp>

#include <util/AtlasPacker.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Texture;
struct LightmapUVMap;
class LightmapJob;
class RenderProxyLightmapVolume;

HYP_ENUM()
enum LightmapTextureType : uint32
{
    LTT_INVALID = ~0u,

    LTT_RADIANCE = 0,
    LTT_IRRADIANCE,

    LTT_MAX
};

HYP_STRUCT(NoScriptBindings)
struct LightmapElement
{
    using Id = uint32;

    HYP_FIELD(Serialize = true)
    uint32 id = ~0u;

    HYP_FIELD(Serialize = true)
    Vec2f offsetUv;

    HYP_FIELD(Serialize = true)
    Vec2u offsetCoords;

    HYP_FIELD(Serialize = true)
    Vec2u dimensions;

    HYP_FIELD(Serialize = true)
    Vec2f scale;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsValid() const
    {
        return id != ~0u;
    }

    HYP_FORCE_INLINE uint16 GetAtlasIndex() const
    {
        return uint16((id >> 16) & 0xFFFFu);
    }

    HYP_FORCE_INLINE uint16 GetElementIndex() const
    {
        return uint16(id & 0xFFFFu);
    }

    static inline void GetAtlasAndElementIndex(Id elementId, uint16& outAtlasIndex, uint16& outElementIndex)
    {
        outAtlasIndex = uint16((elementId >> 16) & 0xFFFFu);
        outElementIndex = uint16(elementId & 0xFFFFu);
    }
};

HYP_STRUCT()
struct LightmapVolumeAtlas : AtlasPacker<LightmapElement>
{
    HYP_PROPERTY(AtlasDimensions, &LightmapVolumeAtlas::atlasDimensions)
    HYP_PROPERTY(Elements, &LightmapVolumeAtlas::elements)
    HYP_PROPERTY(FreeSpaces, &LightmapVolumeAtlas::freeSpaces)

    LightmapVolumeAtlas() = default;

    LightmapVolumeAtlas(const Vec2u& atlasDimensions)
        : AtlasPacker<LightmapElement>(atlasDimensions)
    {
    }

    LightmapVolumeAtlas(const LightmapVolumeAtlas& other) = default;
    LightmapVolumeAtlas(LightmapVolumeAtlas&& other) noexcept = default;

    LightmapVolumeAtlas& operator=(const LightmapVolumeAtlas& other) = default;
    LightmapVolumeAtlas& operator=(LightmapVolumeAtlas&& other) noexcept = default;
};

HYP_CLASS()
class HYP_API LightmapVolume final : public Entity
{
    HYP_OBJECT_BODY(LightmapVolume);

public:
    // maximum number of atlases per LightmapVolume
    static constexpr uint32 s_maxAtlases = 4;

    LightmapVolume();

    LightmapVolume(const BoundingBox& aabb);

    LightmapVolume(const LightmapVolume& other) = delete;
    LightmapVolume& operator=(const LightmapVolume& other) = delete;
    ~LightmapVolume() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_FORCE_INLINE Span<const Handle<Texture>> GetAtlasTextures(LightmapTextureType type) const
    {
        AssertDebug(type < LTT_MAX);

        switch (type)
        {
        case LTT_RADIANCE:
            return m_radianceAtlasTextures;
        case LTT_IRRADIANCE:
            return m_irradianceAtlasTextures;
        default:
            break;
        }

        return {};
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Texture>& GetAtlasTexture(uint16 atlasIndex, LightmapTextureType type) const
    {
        AssertDebug(type < LTT_MAX, "Invalid LightmapTextureType!");

        if (atlasIndex >= m_atlases.Size())
        {
            AssertDebug(false, "atlas index out of bounds");

            return Handle<Texture>::Null();
        }

        switch (type)
        {
        case LTT_RADIANCE:
            AssertDebug(atlasIndex < m_radianceAtlasTextures.Size());
            return m_radianceAtlasTextures[atlasIndex];
        case LTT_IRRADIANCE:
            AssertDebug(atlasIndex < m_irradianceAtlasTextures.Size());
            return m_irradianceAtlasTextures[atlasIndex];
        default:
            return Handle<Texture>::Null();
        }
    }

    HYP_FORCE_INLINE const LightmapVolumeAtlas& GetAtlas(uint16 atlasIndex) const
    {
        AssertDebug(atlasIndex < m_atlases.Size());
        return m_atlases[atlasIndex];
    }

    /*! \brief Add a LightmapElement to this volume. */
    bool AddElement(const LightmapUVMap& uvMap, LightmapElement& outElement, bool shrinkToFit = true, float downscaleLimit = 0.1f);

    const LightmapElement* GetElement(LightmapElement::Id elementId) const;

    bool BuildElementTextures(const LightmapUVMap& uvMap, LightmapElement::Id elementId);
    
    void UpdateRenderProxy(RenderProxyLightmapVolume* proxy);

private:
    void Init() override;

    void UpdateAtlasTextures(
        uint16 atlasIndex,
        HashMap<LightmapElement::Id, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures);

    HYP_FIELD(Serialize = true)
    UUID m_uuid;

    HYP_FIELD(Serialize = true)
    BoundingBox m_aabb;

    HYP_FIELD(Serialize = true)
    Array<Handle<Texture>, FixedAllocator<s_maxAtlases>> m_radianceAtlasTextures;

    HYP_FIELD(Serialize = true)
    Array<Handle<Texture>, FixedAllocator<s_maxAtlases>> m_irradianceAtlasTextures;

    HYP_FIELD(Serialize = true)
    Array<LightmapVolumeAtlas, DynamicAllocator> m_atlases;
};

} // namespace hyperion

