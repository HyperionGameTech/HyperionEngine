/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/containers/Array.hpp>

#include <Core/utilities/Optional.hpp>
#include <Core/utilities/Result.hpp>

#include <asset/AssetObject.hpp>

#include <ui/font/FontFace.hpp>
#include <ui/font/Glyph.hpp>

#include <Core/json/JSON.hpp>

#include <util/img/Bitmap.hpp>

namespace Hyperion {

class Texture;
using FontAtlasBitmap = Bitmap_R8;

HYP_STRUCT()
struct HYP_API FontAtlasTextureSet
{
    HYP_STRUCT_BODY(FontAtlasTextureSet);

    HYP_FIELD()
    Handle<Texture> mainAtlas;
    
    HYP_FIELD()
    Array<Pair<uint32, Handle<Texture>>> atlases;

    ~FontAtlasTextureSet();

    HYP_FORCE_INLINE const Handle<Texture>& GetMainAtlas() const
    {
        return mainAtlas;
    }

    const Handle<Texture>& GetAtlasForPixelSize(uint32 pixelSize) const;

    void AddAtlas(uint32 pixelSize, Handle<Texture> texture, bool isMainAtlas = false);
};

HYP_CLASS(AssetBucket = "FontAtlases")
class HYP_API FontAtlas : public AssetObject
{
    HYP_OBJECT_BODY(FontAtlas);

public:
    static constexpr uint32 SymbolColumns = 20;
    static constexpr uint32 SymbolRows = 5;

    static Array<uint32> GetDefaultSymbolList();

    FontAtlas() = default;

    explicit FontAtlas(Name name)
        : AssetObject(name)
    {
    }

    FontAtlas(Name name, const RC<FontFace>& face);

    FontAtlas(
        Name name,
        const FontAtlasTextureSet& atlasTextures,
        Vec2i cellDimensions,
        const Array<GlyphMetrics>& glyphMetrics,
        Array<uint32> symbolList = GetDefaultSymbolList());

    FontAtlas(const FontAtlas& other) = delete;
    FontAtlas& operator=(const FontAtlas& other) = delete;

    FontAtlas(FontAtlas&& other) noexcept = delete;
    FontAtlas& operator=(FontAtlas&& other) noexcept = delete;

    ~FontAtlas();

    Result RenderAtlasTextures(float mainAtlasScale, float maxScale, float step = 0.1f);

    HYP_FORCE_INLINE const Array<GlyphMetrics>& GetGlyphMetrics() const
    {
        return m_glyphMetrics;
    }

    HYP_FORCE_INLINE const FontAtlasTextureSet& GetAtlasTextures() const
    {
        return m_atlasTextures;
    }

    HYP_FORCE_INLINE const Vec2i& GetCellDimensions() const
    {
        return m_cellDimensions;
    }

    HYP_FORCE_INLINE const Array<uint32>& GetSymbolList() const
    {
        return m_symbolList;
    }

    Optional<const GlyphMetrics&> GetGlyphMetricsForChar(uint32 symbol) const;

    HYP_DEPRECATED_BECAUSE("Using reflection system to serialize instead")
        JSON::Value GenerateMetadataJSON(const String& outputDirectory) const;

private:
    Vec2i FindMaxDimensions(const RC<FontFace>& face) const;

    RC<FontFace> m_face;

    HYP_FIELD()
    FontAtlasTextureSet m_atlasTextures;

    HYP_FIELD()
    Vec2i m_cellDimensions;

    HYP_FIELD()
    Array<GlyphMetrics> m_glyphMetrics;

    HYP_FIELD()
    Array<uint32> m_symbolList;
};

} // namespace Hyperion
