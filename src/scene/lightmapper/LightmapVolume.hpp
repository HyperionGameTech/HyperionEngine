/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAP_VOLUME_HPP
#define HYPERION_LIGHTMAP_VOLUME_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/utilities/UUID.hpp>

#include <core/Handle.hpp>

#include <util/AtlasPacker.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

class Texture;
class RenderLightmapVolume;
struct LightmapUVMap;

// @TODO: Create RenderLightmapVolume, and add it to the RenderState.
// Any visible objects that have a LightmapElementComponent with `volume` of this LightmapVolume
// should be bound to the RenderState, so we ensure the proper textures are available at render time.

// Could also group any rendered objects that have lightmaps to render with that lightmap bound globally.

HYP_ENUM()
enum class LightmapElementTextureType : uint32
{
    RADIANCE = 0,
    IRRADIANCE,

    MAX,

    INVALID = ~0u
};

HYP_STRUCT()

struct LightmapElementTextureEntry
{
    HYP_FIELD(Property = "TextureType", Serialize = true)
    LightmapElementTextureType type = LightmapElementTextureType::INVALID;

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
    Vec2f offset_uv;

    HYP_FIELD(Property = "OffsetCoords", Serialize = true)
    Vec2u offset_coords;

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
    HYP_PROPERTY(AtlasDimensions, &LightmapVolumeAtlas::atlas_dimensions)
    HYP_PROPERTY(Elements, &LightmapVolumeAtlas::elements)
    HYP_PROPERTY(FreeSpaces, &LightmapVolumeAtlas::free_spaces)

    LightmapVolumeAtlas() = default;

    LightmapVolumeAtlas(const Vec2u& atlas_dimensions)
        : AtlasPacker<LightmapElement>(atlas_dimensions)
    {
    }

    LightmapVolumeAtlas(const LightmapVolumeAtlas& other) = default;
    LightmapVolumeAtlas(LightmapVolumeAtlas&& other) noexcept = default;

    LightmapVolumeAtlas& operator=(const LightmapVolumeAtlas& other) = default;
    LightmapVolumeAtlas& operator=(LightmapVolumeAtlas&& other) noexcept = default;
};

HYP_CLASS()
class HYP_API LightmapVolume : public HypObject<LightmapVolume>
{
    HYP_OBJECT_BODY(LightmapVolume);

public:
    LightmapVolume();

    LightmapVolume(const BoundingBox& aabb);

    LightmapVolume(const LightmapVolume& other) = delete;
    LightmapVolume& operator=(const LightmapVolume& other) = delete;
    ~LightmapVolume();

    HYP_FORCE_INLINE RenderLightmapVolume& GetRenderResource() const
    {
        return *m_render_resource;
    }

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

    HYP_FORCE_INLINE const HashMap<LightmapElementTextureType, Handle<Texture>>& GetAtlasTextures() const
    {
        return m_atlas_textures;
    }

    HYP_FORCE_INLINE const LightmapVolumeAtlas& GetAtlas() const
    {
        return m_atlas;
    }

    /*! \brief Add a LightmapElement to this volume. */
    bool AddElement(const LightmapUVMap& uv_map, LightmapElement& out_element, bool shrink_to_fit = true, float downscale_limit = 0.1f);

    const LightmapElement* GetElement(uint32 index) const;

    bool BuildElementTextures(const LightmapUVMap& uv_map, uint32 index);

    void Init() override;

private:
    void UpdateAtlasTextures();

    RenderLightmapVolume* m_render_resource;

    HYP_FIELD(Serialize = true)
    UUID m_uuid;

    HYP_FIELD(Serialize = true)
    BoundingBox m_aabb;

    HYP_FIELD(Serialize = true)
    HashMap<LightmapElementTextureType, Handle<Texture>> m_atlas_textures;
    
    HYP_FIELD(Serialize = true)
    LightmapVolumeAtlas m_atlas;
};

} // namespace hyperion

#endif
