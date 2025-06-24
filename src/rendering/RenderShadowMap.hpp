/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SHADOW_MAP_HPP
#define HYPERION_RENDERING_SHADOW_MAP_HPP

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Matrix4.hpp>
#include <core/math/Vector2.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/IDGenerator.hpp>

#include <util/AtlasPacker.hpp>

#include <Types.hpp>

namespace hyperion {

struct ShadowMapShaderData
{
    Matrix4 projection;
    Matrix4 view;
    Vec4f aabb_max;
    Vec4f aabb_min;
    Vec4f dimensions_scale; // xy = shadow map dimensions in pixels, zw = shadow map dimensions relative to the atlas dimensions
    Vec2f offset_uv;        // offset in the atlas texture array
    uint32 layer_index;     // index of the atlas in the shadow map texture array, or cubemap index for point lights
    uint32 flags;

    Vec4f _pad1;
    Vec4f _pad2;
    Vec4f _pad3;
    Vec4f _pad4;
};

static_assert(sizeof(ShadowMapShaderData) == 256);

/* max number of shadow maps, based on size in kb */
static const SizeType max_shadow_maps = (4ull * 1024ull) / sizeof(ShadowMapShaderData);

class RenderWorld;
class RenderShadowMap;

enum ShadowMapFilter : uint32
{
    SMF_STANDARD,
    SMF_PCF,
    SMF_CONTACT_HARDENED,
    SMF_VSM,

    SMF_MAX
};

enum ShadowFlags : uint32
{
    SF_NONE = 0x0,
    SF_PCF = 0x1,
    SF_VSM = 0x2,
    SF_CONTACT_HARDENED = 0x4
};

HYP_MAKE_ENUM_FLAGS(ShadowFlags)

enum ShadowMapType : uint32
{
    SMT_DIRECTIONAL,
    SMT_SPOT,
    SMT_OMNI
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

class ShadowMapAllocator
{
public:
    ShadowMapAllocator();
    ~ShadowMapAllocator();

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

    RenderShadowMap* AllocateShadowMap(ShadowMapType shadow_map_type, ShadowMapFilter filter_mode, const Vec2u& dimensions);
    bool FreeShadowMap(RenderShadowMap* shadow_map);

private:
    Vec2u m_atlas_dimensions;
    Array<ShadowMapAtlas> m_atlases;

    ImageRef m_atlas_image;
    ImageViewRef m_atlas_image_view;

    ImageRef m_point_light_shadow_map_image;
    ImageViewRef m_point_light_shadow_map_image_view;

    IDGenerator m_point_light_shadow_map_id_generator;
};

class RenderShadowMap final : public RenderResourceBase
{
public:
    RenderShadowMap(ShadowMapType type, ShadowMapFilter filter_mode, const ShadowMapAtlasElement& atlas_element, const ImageViewRef& image_view);
    RenderShadowMap(RenderShadowMap&& other) noexcept;
    virtual ~RenderShadowMap() override;

    HYP_FORCE_INLINE ShadowMapType GetShadowMapType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE ShadowMapFilter GetFilterMode() const
    {
        return m_filter_mode;
    }

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_atlas_element.dimensions;
    }

    HYP_FORCE_INLINE const ShadowMapAtlasElement& GetAtlasElement() const
    {
        return m_atlas_element;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetImageView() const
    {
        return m_image_view;
    }

    void SetBufferData(const ShadowMapShaderData& buffer_data);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    ShadowMapType m_type;
    ShadowMapFilter m_filter_mode;
    ShadowMapAtlasElement m_atlas_element;
    ImageViewRef m_image_view;
    ShadowMapShaderData m_buffer_data;
};

} // namespace hyperion

#endif