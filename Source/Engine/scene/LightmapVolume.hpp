/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>

#include <Core/math/BoundingBox.hpp>

#include <Core/utilities/Uuid.hpp>

#include <scene/Volume.hpp>

#include <util/AtlasPacker.hpp>

namespace Hyperion {

class Texture;
class LightmapVolume;
class RenderProxyLightmapVolume;

namespace Baking {

template <class T>
class BakeData;

} // namespace Baking

HYP_ENUM()
enum LightmapTextureType : uint32
{
    LTT_INVALID = ~0u,

    LTT_RADIANCE = 0,
    LTT_IRRADIANCE,

    LTT_MAX
};

enum class LightmapElementId : uint32;
static constexpr LightmapElementId InvalidLightmapElementId = LightmapElementId(~0u);

HYP_STRUCT(NoScriptBindings)
struct LightmapElement
{
    HYP_STRUCT_BODY(LightmapElement);

    HYP_FIELD(Serialize = true)
    LightmapElementId id = InvalidLightmapElementId;

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
        return id != InvalidLightmapElementId;
    }

    HYP_FORCE_INLINE uint16 GetAtlasIndex() const
    {
        return uint16((uint32(id) >> 16) & 0xFFFFu);
    }

    HYP_FORCE_INLINE uint16 GetElementIndex() const
    {
        return uint16(uint32(id) & 0xFFFFu);
    }

    static constexpr inline void GetAtlasAndElementIndex(LightmapElementId elementId, uint16& outAtlasIndex, uint16& outElementIndex)
    {
        outAtlasIndex = uint16((uint32(elementId) >> 16) & 0xFFFFu);
        outElementIndex = uint16(uint32(elementId) & 0xFFFFu);
    }
};

HYP_STRUCT()
struct LightmapVolumeAtlas : AtlasPacker<LightmapElement>
{
    HYP_STRUCT_BODY(LightmapVolumeAtlas);

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
class HYP_API LightmapVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(LightmapVolume);

public:
    // maximum number of atlases per LightmapVolume
    static constexpr uint32 MaxAtlases = 4;
    static constexpr Vec2u DefaultAtlasDimensions = Vec2u(2048, 2048);

    LightmapVolume();

    explicit LightmapVolume(const BoundingBox& localBounds);

    LightmapVolume(const LightmapVolume& other) = delete;
    LightmapVolume& operator=(const LightmapVolume& other) = delete;
    ~LightmapVolume() override;

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
    const Handle<Texture>& GetAtlasTexture(uint16 atlasIndex, LightmapTextureType type) const;

    HYP_METHOD()
    void SetAtlasTexture(uint16 atlasIndex, LightmapTextureType type, const Handle<Texture>& texture);

    HYP_FORCE_INLINE const LightmapVolumeAtlas& GetAtlas(uint16 atlasIndex) const
    {
        AssertDebug(atlasIndex < m_atlases.Size());
        return m_atlases[atlasIndex];
    }

    HYP_FORCE_INLINE Span<LightmapVolumeAtlas> GetAtlases()
    {
        return m_atlases.ToSpan();
    }

    HYP_FORCE_INLINE Span<const LightmapVolumeAtlas> GetAtlases() const
    {
        return m_atlases.ToSpan();
    }

    /*! \brief Add a LightmapElement to this volume. */
    bool AddElement(Vec2u dimensions, LightmapElement& outElement, bool shrinkToFit = true, float downscaleLimit = 0.1f);

    const LightmapElement* GetElement(LightmapElementId elementId) const;

    void UpdateRenderProxy(RenderProxyLightmapVolume* proxy);

#if HYP_EDITOR
    HYP_METHOD(EditorOnly, EditAction = "BakeLightmaps")
    void BakeLightmaps();
#endif

protected:
    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

private:
    void Init() override;

    void UpdateAtlasTextures(
        uint16 atlasIndex,
        HashMap<LightmapElementId, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures);

    HYP_FIELD(Property = "RadianceAtlasTextures")
    Array<Handle<Texture>, FixedAllocator<MaxAtlases>> m_radianceAtlasTextures;

    HYP_FIELD(Property = "IrradianceAtlasTextures")
    Array<Handle<Texture>, FixedAllocator<MaxAtlases>> m_irradianceAtlasTextures;

    HYP_FIELD(Property = "Atlases")
    Array<LightmapVolumeAtlas, DynamicAllocator> m_atlases;
};

constexpr uint8 LightmapStencilMask = (1u << LightmapVolume::MaxAtlases) - 1;

} // namespace Hyperion
