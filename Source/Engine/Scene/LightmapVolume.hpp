/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Utilities/Traits.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <Scene/Volume.hpp>

#include <Util/AtlasPacker.hpp>

#include <Rendering/Shared.hpp>

namespace Hyperion {

class Texture;
class LightmapVolume;
struct RenderProxyLightmapVolume;

namespace Baking {

template <class T>
class BakeData;

} // namespace Baking

enum class LightmapElementId : uint32;
static constexpr LightmapElementId InvalidLightmapElementId = Invalid<LightmapElementId>;

HYP_STRUCT(NoScriptBindings)
struct LightmapElement
{
    HYP_STRUCT_BODY(LightmapElement);

    HYP_FIELD(Serialize = true)
    LightmapElementId id = InvalidLightmapElementId;

    HYP_FIELD(Serialize = true)
    Vec2f offsetUV;

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
class ENGINE_API LightmapVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(LightmapVolume);

public:
    static constexpr Vec2u DefaultAtlasDimensions = Vec2u(2048, 2048);

    enum AtlasTextureType : uint8
    {
        RadianceTexture = 0,
        IrradianceTexture,
        Max
    };

    static constexpr uint32 NumAtlasTextureTypes = static_cast<uint8>(AtlasTextureType::Max);

    LightmapVolume();

    explicit LightmapVolume(const BoundingBox& localBounds);

    LightmapVolume(const LightmapVolume& other) = delete;
    LightmapVolume& operator=(const LightmapVolume& other) = delete;
    ~LightmapVolume() override;

    HYP_FORCE_INLINE Span<const Handle<Texture>> GetAtlasTextures(AtlasTextureType type) const
    {
        switch (type)
        {
        case RadianceTexture:
            return m_radianceAtlasTextures;
        case IrradianceTexture:
            return m_irradianceAtlasTextures;
        default:
            return {};
        }
    }

    const Handle<Texture>& GetAtlasTexture(uint16 atlasIndex, AtlasTextureType type) const;
    void SetAtlasTexture(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture);

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
    bool AddElement(Vec2u dimensions, LightmapElement*& outElement, bool shrinkToFit = true, float downscaleLimit = 0.1f);

    const LightmapElement* GetElement(LightmapElementId elementId) const;

    /*! \brief Remove all lightmap elements from this volume */
    void RemoveAllElements();

    void UpdateRenderProxy(RenderProxyLightmapVolume* proxy);

#if HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Bake Lightmap")
    void Rebake();
#endif

protected:
    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

private:
    HYP_FIELD(Property = "RadianceAtlasTextures")
    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> m_radianceAtlasTextures;

    HYP_FIELD(Property = "IrradianceAtlasTextures")
    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> m_irradianceAtlasTextures;

    HYP_FIELD(Property = "Atlases")
    Array<LightmapVolumeAtlas> m_atlases;
};

constexpr uint8 LightmapStencilMask = (1u << MaxAtlasesPerLightmapVolume) - 1;

} // namespace Hyperion
