/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <core/math/Matrix4.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/IdGenerator.hpp>

#include <util/AtlasPacker.hpp>

#include <core/Types.hpp>

namespace hyperion {

class FullScreenPass;
class ShadowMap;
enum ShadowMapFilter : uint32;
enum ShadowMapType : uint32;

HYP_STRUCT()
struct ShadowMapAtlasElement
{
    // Directional and spot lights only: index of the atlas in the shadow map texture array
    uint32 atlasIndex = ~0u;

    // Point light shadow maps only: index of the cubemap in the texture array
    uint32 pointLightIndex = ~0u;

    // Index of the element in the atlas
    uint32 index = ~0u;

    // Offset in the atlas texture array, in uv space
    Vec2f offsetUv;

    // Offset in the atlas texture array, in pixels
    Vec2u offsetCoords;

    // Dimensions of the shadow map in pixels
    Vec2u dimensions;

    // Shadow map dimensions relative to the atlas dimensions
    Vec2f scale;

    HYP_FORCE_INLINE bool operator==(const ShadowMapAtlasElement& other) const
    {
        return atlasIndex == other.atlasIndex
            && pointLightIndex == other.pointLightIndex
            && index == other.index
            && offsetUv == other.offsetUv
            && offsetCoords == other.offsetCoords
            && dimensions == other.dimensions
            && scale == other.scale;
    }

    HYP_FORCE_INLINE bool operator!=(const ShadowMapAtlasElement& other) const
    {
        return atlasIndex != other.atlasIndex
            || pointLightIndex != other.pointLightIndex
            || index != other.index
            || offsetUv != other.offsetUv
            || offsetCoords != other.offsetCoords
            || dimensions != other.dimensions
            || scale != other.scale;
    }
};

HYP_STRUCT()
struct ShadowMapAtlas : AtlasPacker<ShadowMapAtlasElement>
{
    HYP_PROPERTY(AtlasDimensions, &ShadowMapAtlas::atlasDimensions)
    HYP_PROPERTY(Elements, &ShadowMapAtlas::elements)
    HYP_PROPERTY(FreeSpaces, &ShadowMapAtlas::freeSpaces)

    HYP_FIELD(Property = "AtlasIndex", Serialize = true)
    uint32 atlasIndex;

    ShadowMapAtlas()
        : AtlasPacker<ShadowMapAtlasElement>(Vec2u(0, 0)),
          atlasIndex(~0u)
    {
    }

    ShadowMapAtlas(uint32 atlasIndex, const Vec2u& atlasDimensions)
        : AtlasPacker(atlasDimensions),
          atlasIndex(atlasIndex)
    {
    }

    ShadowMapAtlas(const ShadowMapAtlas& other)
        : AtlasPacker(static_cast<const AtlasPacker<ShadowMapAtlasElement>&>(other)),
          atlasIndex(other.atlasIndex)
    {
    }

    ShadowMapAtlas(ShadowMapAtlas&& other) noexcept
        : AtlasPacker(std::move(static_cast<AtlasPacker<ShadowMapAtlasElement>&&>(other))),
          atlasIndex(other.atlasIndex)
    {
        other.atlasIndex = ~0u;
    }

    ShadowMapAtlas& operator=(const ShadowMapAtlas& other)
    {
        if (this == &other)
        {
            return *this;
        }

        AtlasPacker<ShadowMapAtlasElement>::operator=(other);
        atlasIndex = other.atlasIndex;

        return *this;
    }

    ShadowMapAtlas& operator=(ShadowMapAtlas&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        AtlasPacker<ShadowMapAtlasElement>::operator=(std::move(other));

        atlasIndex = other.atlasIndex; // NOLINT(bugprone-use-after-move)
        other.atlasIndex = ~0u;

        return *this;
    }

    bool AddElement(const Vec2u& elementDimensions, ShadowMapAtlasElement& outElement);
};

class ShadowMapAllocator
{
public:
    ShadowMapAllocator();
    ~ShadowMapAllocator();

    HYP_FORCE_INLINE const GpuImageRef& GetAtlasImage() const
    {
        return m_atlasImage;
    }

    HYP_FORCE_INLINE const GpuImageViewRef& GetAtlasImageView() const
    {
        return m_atlasImageView;
    }

    HYP_FORCE_INLINE const GpuImageRef& GetPointLightShadowMapImage() const
    {
        return m_pointLightShadowMapImage;
    }

    HYP_FORCE_INLINE const GpuImageViewRef& GetPointLightShadowMapImageView() const
    {
        return m_pointLightShadowMapImageView;
    }

    void Initialize();
    void Destroy();

    ShadowMap* AllocateShadowMap(ShadowMapType shadowMapType, ShadowMapFilter filterMode, const Vec2u& dimensions);
    bool FreeShadowMap(ShadowMap* shadowMap);

private:
    Vec2u m_atlasDimensions;
    Array<ShadowMapAtlas> m_atlases;

    GpuImageRef m_atlasImage;
    GpuImageViewRef m_atlasImageView;

    GpuImageRef m_pointLightShadowMapImage;
    GpuImageViewRef m_pointLightShadowMapImageView;

    IdGenerator m_pointLightShadowMapIdGenerator;
};

} // namespace hyperion
