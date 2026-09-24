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
#include <Core/Utilities/Optional.hpp>

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

enum class LightmapVolumeId : uint32;
static constexpr LightmapVolumeId InvalidLightmapVolumeId = Invalid<LightmapVolumeId>;

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

    static constexpr inline LightmapElementId MakeId(uint16 atlasIndex, uint16 elementIndex)
    {
        return LightmapElementId((uint32(atlasIndex) << 16) | uint32(elementIndex));
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
    static constexpr LightmapVolumeId InvalidId = InvalidLightmapVolumeId;

    // Entity rects are packed into 12 bits per axis for EntityShaderData, see GetEntityLightmapRect()
    static_assert(DefaultAtlasDimensions.x <= 4096 && DefaultAtlasDimensions.y <= 4096);
    static_assert(MathUtil::IsPowerOfTwo(DefaultAtlasDimensions.x) && MathUtil::IsPowerOfTwo(DefaultAtlasDimensions.y));

    enum AtlasTextureType : uint8
    {
        IrradianceTexture = 0,
        BentNormalTexture,

        Max
    };

    static constexpr uint32 NumAtlasTextureTypes = uint8(AtlasTextureType::Max);
    static constexpr const char* TextureTypeNames[NumAtlasTextureTypes] = { "IR", "BN" };

    LightmapVolume();

    explicit LightmapVolume(const BoundingBox& localBounds);

    LightmapVolume(const LightmapVolume& other) = delete;
    LightmapVolume& operator=(const LightmapVolume& other) = delete;
    ~LightmapVolume() override;

    HYP_METHOD()
    void SetName(Name name) override;

    HYP_METHOD(Property = "LightmapVolumeId", Editor = false, Serialize = true, NoSwatchOverride)
    LightmapVolumeId GetLightmapVolumeId() const
    {
        return m_id;
    }

    HYP_METHOD(Property = "LightmapVolumeId", Editor = false, Serialize = true, NoSwatchOverride)
    void SetLightmapVolumeId(LightmapVolumeId id);

    HYP_FORCE_INLINE Span<const Handle<Texture>> GetAtlasTextures(AtlasTextureType type) const
    {
        switch (type)
        {
        case IrradianceTexture:
            return m_irradianceAtlasTextures;
        case BentNormalTexture:
            return m_bentNormalAtlasTextures;
        default:
            return {};
        }
    }

    const Handle<Texture>& GetAtlasTexture(uint16 atlasIndex, AtlasTextureType type) const;
    void SetAtlasTexture(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture);

    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> GetAtlasTexturesForSwatch(AtlasTextureType type, Name swatchName) const;

    void SetAtlasTextureForSwatch(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture, Name swatchName);
    void ClearSwatchAtlasTextureOverrides(uint32 preserveTextureTypesMask = 0);

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly)
    Array<Name> GetBakedSwatchNames() const;
#endif // HYP_EDITOR

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

    /*! \brief Lightmap texels per world unit that a bake aims for. Scaled down when the owned entities don't fit the atlases. */
    HYP_METHOD(Property = "TexelsPerUnit", Serialize = true, Editor = true, NoSwatchOverride)
    float GetTexelsPerUnit() const
    {
        return m_texelsPerUnit;
    }

    HYP_METHOD(Property = "TexelsPerUnit", Serialize = true, Editor = true, NoSwatchOverride)
    void SetTexelsPerUnit(float texelsPerUnit);

    /*! \brief TexelsPerUnit the current packing was built with; a different value forces the next bake to repack. */
    HYP_FORCE_INLINE float GetPackedTexelsPerUnit() const
    {
        return m_packedTexelsPerUnit;
    }

    /*! \brief World space bounds of everything this volume lights: its own bounds and those of the entities it owns. */
    BoundingBox GetLightingBounds() const;

    HYP_FORCE_INLINE const BoundingBox& GetCoverageBounds() const
    {
        return m_coverageBounds;
    }

    void SetCoverageBounds(const BoundingBox& coverageBounds);

    /*! \brief First stencil value of this volume's atlas pages, 0 if none could be assigned. Assigned by LightmapSystem. */
    HYP_FORCE_INLINE uint8 GetStencilBase() const
    {
        return m_stencilBase;
    }

    void SetStencilBase(uint8 stencilBase);

    /*! \brief Stencil value lightmapped pixels in \p atlasIndex write, which LightmapPass tests to route them to this volume's atlas. */
    HYP_FORCE_INLINE uint8 GetStencilValue(uint16 atlasIndex) const
    {
        return m_stencilBase != 0 ? uint8(m_stencilBase + atlasIndex) : 0;
    }

    /*! \brief Pages that have at least one element packed into them. */
    uint32 NumUsedAtlases() const;

    /*! \brief Add a LightmapElement to this volume. */
    bool AddElement(Vec2u dimensions, LightmapElement*& outElement, bool shrinkToFit = true, float downscaleLimit = 0.1f);

    const LightmapElement* GetElement(LightmapElementId elementId) const;

    /*! \brief Pack an entity's rect for EntityShaderData: texel offset + slot in \p outRectOffset, uv scale in texels + atlas size in \p outRectSize.
     *  Returns false if the element doesn't exist or can't be routed (no stencil value). */
    bool GetEntityLightmapRect(LightmapElementId elementId, uint32& outRectOffset, uint32& outRectSize, uint8& outStencilValue) const;

    /*! \brief Remove all lightmap elements from this volume, except for those which are of types that the \p preserveTextureTypesMask bitmask includes (where each bit = 1<<type) */
    void RemoveAllElements(uint32 preserveTextureTypesMask = 0);

    /*! \brief Replace the packing with one built off the sim thread. Textures must be rebuilt for it afterwards. */
    void SetPacking(Array<LightmapVolumeAtlas>&& atlases, float packedTexelsPerUnit);

    void UpdateRenderProxy(RenderProxyLightmapVolume* proxy);

    static Name GetAtlasTexturesPropertyName(AtlasTextureType type);

#pragma region Actions

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Bake Lightmap")
    void BakeLightmap();

    HYP_METHOD(EditorOnly, EditorAction = "Bake Bent Normals")
    void BakeBentNormals();
#endif // HYP_EDITOR

#pragma endregion Actions

protected:
    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

private:
    HYP_FORCE_INLINE FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume>& GetAtlasTexturesArray(AtlasTextureType textureType)
    {
        switch (textureType)
        {
        case IrradianceTexture:
            return m_irradianceAtlasTextures;
        case BentNormalTexture:
            return m_bentNormalAtlasTextures;
        default:
            HYP_UNREACHABLE();
        }
    }

    HYP_FIELD(Property = "IrradianceAtlasTextures", Serialize, Editor = false)
    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> m_irradianceAtlasTextures;

    HYP_FIELD(Property = "BentNormalAtlasTextures", Serialize, Editor = false)
    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> m_bentNormalAtlasTextures;

    HYP_FIELD(Property = "Atlases", Serialize, Editor = false, NoSwatchOverride)
    Array<LightmapVolumeAtlas> m_atlases;

    HYP_FIELD(Property = "PackedTexelsPerUnit", Serialize, Editor = false, NoSwatchOverride)
    float m_packedTexelsPerUnit = 0.0f;

    HYP_FIELD(Property = "CoverageBounds", Serialize, Editor = false, NoSwatchOverride)
    BoundingBox m_coverageBounds;

    HYP_FIELD(Property = "LightmapVolumeId", Editor = false, Serialize, NoSwatchOverride)
    LightmapVolumeId m_id;

    float m_texelsPerUnit = 8.0f;

    uint8 m_stencilBase = 0;
};

} // namespace Hyperion
