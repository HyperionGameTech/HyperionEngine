/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADOWS_HPP
#define HYPERION_SHADOWS_HPP

#include <core/math/Vector2.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/IDGenerator.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <util/AtlasPacker.hpp>

#include <Types.hpp>

namespace hyperion {

class RenderShadowMap;

enum class ShadowMapFilterMode : uint32
{
    STANDARD,
    PCF,
    CONTACT_HARDENED,
    VSM,

    MAX
};

enum class ShadowFlags : uint32
{
    NONE = 0x0,
    PCF = 0x1,
    VSM = 0x2,
    CONTACT_HARDENED = 0x4
};

HYP_MAKE_ENUM_FLAGS(ShadowFlags)

enum class ShadowMapType : uint32
{
    DIRECTIONAL_SHADOW_MAP,
    SPOT_SHADOW_MAP,
    POINT_SHADOW_MAP,

    MAX
};

struct ShadowMapAtlasElement
{
    // Directional and spot lights only: index of the atlas in the shadow map texture array
    uint32 atlas_index = ~0u;

    // Point light shadow maps only: index of the cubemap in the texture array
    uint32 point_light_index = ~0u;

    // Index of the element in the atlas
    uint32 index = ~0u;

    // Offset in the atlas texture array, in uv space
    Vec2f offset_uv;

    // Offset in the atlas texture array, in pixels
    Vec2u offset_coords;

    // Dimensions of the shadow map in pixels
    Vec2u dimensions;

    // Shadow map dimensions relative to the atlas dimensions
    Vec2f scale;

    HYP_FORCE_INLINE bool operator==(const ShadowMapAtlasElement& other) const
    {
        return atlas_index == other.atlas_index
            && point_light_index == other.point_light_index
            && index == other.index
            && offset_uv == other.offset_uv
            && offset_coords == other.offset_coords
            && dimensions == other.dimensions
            && scale == other.scale;
    }

    HYP_FORCE_INLINE bool operator!=(const ShadowMapAtlasElement& other) const
    {
        return atlas_index != other.atlas_index
            || point_light_index != other.point_light_index
            || index != other.index
            || offset_uv != other.offset_uv
            || offset_coords != other.offset_coords
            || dimensions != other.dimensions
            || scale != other.scale;
    }
};

HYP_STRUCT()

struct ShadowMapAtlas : AtlasPacker<ShadowMapAtlasElement>
{
    HYP_PROPERTY(AtlasDimensions, &ShadowMapAtlas::atlas_dimensions)
    HYP_PROPERTY(Elements, &ShadowMapAtlas::elements)
    HYP_PROPERTY(FreeSpaces, &ShadowMapAtlas::free_spaces)

    HYP_FIELD(Property = "AtlasIndex", Serialize = true)
    uint32 atlas_index;

    ShadowMapAtlas()
        : AtlasPacker<ShadowMapAtlasElement>(Vec2u(0, 0)),
          atlas_index(~0u)
    {
    }

    ShadowMapAtlas(uint32 atlas_index, const Vec2u& atlas_dimensions)
        : AtlasPacker(atlas_dimensions),
          atlas_index(atlas_index)
    {
    }

    ShadowMapAtlas(const ShadowMapAtlas& other)
        : AtlasPacker(static_cast<const AtlasPacker<ShadowMapAtlasElement>&>(other)),
          atlas_index(other.atlas_index)
    {
    }

    ShadowMapAtlas(ShadowMapAtlas&& other) noexcept
        : AtlasPacker(std::move(static_cast<AtlasPacker<ShadowMapAtlasElement>&&>(other))),
          atlas_index(other.atlas_index)
    {
        other.atlas_index = ~0u;
    }

    ShadowMapAtlas& operator=(const ShadowMapAtlas& other)
    {
        if (this == &other)
        {
            return *this;
        }

        AtlasPacker<ShadowMapAtlasElement>::operator=(other);
        atlas_index = other.atlas_index;

        return *this;
    }

    ShadowMapAtlas& operator=(ShadowMapAtlas&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        AtlasPacker<ShadowMapAtlasElement>::operator=(std::move(other));

        atlas_index = other.atlas_index; // NOLINT(bugprone-use-after-move)
        other.atlas_index = ~0u;

        return *this;
    }

    bool AddElement(const Vec2u& element_dimensions, ShadowMapAtlasElement& out_element);
};

class ShadowMapManager
{
public:
    ShadowMapManager();
    ~ShadowMapManager();

    HYP_FORCE_INLINE const ImageRef& GetAtlasImage() const
    {
        return m_atlas_image;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetAtlasImageView() const
    {
        return m_atlas_image_view;
    }

    HYP_FORCE_INLINE const ImageRef& GetPointLightShadowMapImage() const
    {
        return m_point_light_shadow_map_image;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetPointLightShadowMapImageView() const
    {
        return m_point_light_shadow_map_image_view;
    }

    void Initialize();
    void Destroy();

    RenderShadowMap* AllocateShadowMap(ShadowMapType shadow_map_type, ShadowMapFilterMode filter_mode, const Vec2u& dimensions);
    bool FreeShadowMap(RenderShadowMap* shadow_render_map);

private:
    Vec2u m_atlas_dimensions;
    Array<ShadowMapAtlas> m_atlases;

    ImageRef m_atlas_image;
    ImageViewRef m_atlas_image_view;

    ImageRef m_point_light_shadow_map_image;
    ImageViewRef m_point_light_shadow_map_image_view;

    IDGenerator m_point_light_shadow_map_id_generator;
};

} // namespace hyperion

#endif