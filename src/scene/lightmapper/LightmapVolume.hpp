/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/object/Handle.hpp>

#include <scene/Entity.hpp>

#include <util/AtlasPacker.hpp>

#include <util/GameCounter.hpp>
#include <core/Types.hpp>

namespace hyperion {

class Texture;
struct LightmapUVMap;
class RenderProxyLightmapVolume;

HYP_ENUM()
enum LightmapTextureType : uint32
{
    LTT_INVALID = ~0u,

    LTT_RADIANCE = 0,
    LTT_IRRADIANCE,

    LTT_MAX
};

HYP_STRUCT()
struct LightmapElementTextureEntry
{
    HYP_FIELD(Property = "TextureType", Serialize = true)
    LightmapTextureType type = LTT_INVALID;

    HYP_FIELD(Property = "Texture", Serialize = true)
    Handle<Texture> texture;
};

HYP_STRUCT(NoScriptBindings)
struct LightmapElement
{
    HYP_FIELD(Property = "Index", Serialize = true)
    uint32 index = ~0u;

    HYP_FIELD(Property = "Entries", Serialize = true)
    Array<LightmapElementTextureEntry> entries;

    HYP_FIELD(Property = "OffsetUV", Serialize = true)
    Vec2f offsetUv;

    HYP_FIELD(Property = "OffsetCoords", Serialize = true)
    Vec2u offsetCoords;

    HYP_FIELD(Property = "Dimensions", Serialize = true)
    Vec2u dimensions;

    HYP_FIELD(Property = "Scale", Serialize = true)
    Vec2f scale;

    HYP_METHOD()
    bool IsValid() const
    {
        return index != ~0u;
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

    HYP_FORCE_INLINE const FixedArray<Handle<Texture>, LTT_MAX>& GetAtlasTextures() const
    {
        return m_atlasTextures;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Texture>& GetAtlasTexture(LightmapTextureType type) const
    {
        AssertDebug(type < LTT_MAX, "Invalid LightmapTextureType!");

        return m_atlasTextures[type];
    }

    HYP_FORCE_INLINE const LightmapVolumeAtlas& GetAtlas() const
    {
        return m_atlas;
    }

    /*! \brief Add a LightmapElement to this volume. */
    bool AddElement(const LightmapUVMap& uvMap, LightmapElement& outElement, bool shrinkToFit = true, float downscaleLimit = 0.1f);

    const LightmapElement* GetElement(uint32 index) const;

    bool BuildElementTextures(const LightmapUVMap& uvMap, uint32 index);
    
    void UpdateRenderProxy(RenderProxyLightmapVolume* proxy);

private:
    void Init() override;

    void UpdateAtlasTextures();

    HYP_FIELD(Serialize = true)
    UUID m_uuid;

    HYP_FIELD(Serialize = true)
    BoundingBox m_aabb;

    HYP_FIELD(Serialize = true)
    FixedArray<Handle<Texture>, LTT_MAX> m_atlasTextures;

    HYP_FIELD(Serialize = true)
    LightmapVolumeAtlas m_atlas;
};

} // namespace hyperion

