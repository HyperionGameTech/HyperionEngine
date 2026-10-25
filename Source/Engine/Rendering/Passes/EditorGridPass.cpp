/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/EditorGridPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>
#include <Framework/EngineDefaults.hpp>

#include <Asset/AssetRegistry.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Logging/Logger.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statDrawEditorGrid("Rendering/GPU/EditorGrid");

CVar<bool> g_cvEditorGrid { "Editor.ShowGrid", true };
CVar<float> g_cvEditorGridSize { "Editor.Grid.Size", 1.0f };
CVar<float> g_cvEditorGridOffsetX { "Editor.Grid.OffsetX", 0.0f };
CVar<float> g_cvEditorGridOffsetY { "Editor.Grid.OffsetY", 0.0f };
CVar<float> g_cvEditorGridOffsetZ { "Editor.Grid.OffsetZ", 0.0f };

/// digits are drawn as this tall
static constexpr float LabelCapHeightPixels = 10.0f;
static constexpr char LabelGlyphChars[EditorGridLabelFont::NumGlyphs] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    '-', '.',
    'X', 'Y', 'Z'
};

/// how far the baked distance fields reach past each glyph's outline, in pixels of the font atlas
static constexpr int32 GlyphDistanceSpread = 10;

struct EditorGridConstants
{
    Vec4f gridOffsetAndSize;

    /// x == 1 when the glyph atlas is bound, y: label units the glyph distances span
    Vec4f labelParams;

    EditorGridLabelFont::Glyphs glyphs;
};

/// where each glyph sits in the font atlas and in the baked distance field texture. Both the commandlet and
/// the runtime work it out from the font's metrics, so only the texture has to be baked
struct LabelGlyphLayout
{
    EditorGridLabelFont::Glyphs glyphs {};
    float distanceRange = 0.0f;

    Vec2u extent;

    Vec2i sourcePositions[EditorGridLabelFont::NumGlyphs];
    Vec2i sourceSizes[EditorGridLabelFont::NumGlyphs];
    int32 cellOffsets[EditorGridLabelFont::NumGlyphs] = {};

    bool isValid = false;
};

static LabelGlyphLayout ComputeLabelGlyphLayout(const FontAtlas& fontAtlas)
{
    LabelGlyphLayout layout;

    Optional<const GlyphMetrics&> zeroMetrics = fontAtlas.GetGlyphMetricsForChar(uint32('0'));
    const float capHeight = zeroMetrics.HasValue() ? MathUtil::Max(float(zeroMetrics->bearingY), 1.0f) : 46.0f;
    const float scale = LabelCapHeightPixels / capHeight;

    Vec2i cellSizes[EditorGridLabelFont::NumGlyphs];
    Vec2i extent = Vec2i::Zero();

    for (uint32 glyphIndex = 0; glyphIndex < EditorGridLabelFont::NumGlyphs; glyphIndex++)
    {
        Optional<const GlyphMetrics&> metrics = fontAtlas.GetGlyphMetricsForChar(uint32(LabelGlyphChars[glyphIndex]));

        if (!metrics.HasValue() || metrics->width == 0 || metrics->height == 0)
        {
            continue;
        }

        const Vec2i glyphSize = Vec2i(int32(metrics->width), int32(metrics->height));

        layout.sourcePositions[glyphIndex] = metrics->imagePosition;
        layout.sourceSizes[glyphIndex] = glyphSize;
        layout.cellOffsets[glyphIndex] = extent.x;

        cellSizes[glyphIndex] = glyphSize + Vec2i(GlyphDistanceSpread * 2);

        extent.x += cellSizes[glyphIndex].x;
        extent.y = MathUtil::Max(extent.y, cellSizes[glyphIndex].y);

        layout.glyphs.pixelRects[glyphIndex] = Vec4f(
            float(metrics->bearingX - GlyphDistanceSpread) * scale,
            (capHeight - float(metrics->bearingY + GlyphDistanceSpread)) * scale,
            float(cellSizes[glyphIndex].x) * scale,
            float(cellSizes[glyphIndex].y) * scale);

        // advance is 26.6 fixed point
        layout.glyphs.advances[glyphIndex / 4][glyphIndex % 4] = (float(metrics->advance) / 64.0f) * scale;
    }

    if (extent.x == 0)
    {
        return layout;
    }

    for (uint32 glyphIndex = 0; glyphIndex < EditorGridLabelFont::NumGlyphs; glyphIndex++)
    {
        if (layout.sourceSizes[glyphIndex].x == 0)
        {
            continue;
        }

        layout.glyphs.uvRects[glyphIndex] = Vec4f(
            float(layout.cellOffsets[glyphIndex]) / float(extent.x),
            0.0f,
            float(cellSizes[glyphIndex].x) / float(extent.x),
            float(cellSizes[glyphIndex].y) / float(extent.y));
    }

    layout.extent = Vec2u(extent);
    layout.distanceRange = float(GlyphDistanceSpread * 2) * scale;
    layout.isValid = true;

    return layout;
}

#pragma region EditorGridLabelFont

Handle<Texture> EditorGridLabelFont::BakeGlyphTexture(const FontAtlas& fontAtlas)
{
    const LabelGlyphLayout layout = ComputeLabelGlyphLayout(fontAtlas);
    const Handle<Texture>& sourceTexture = fontAtlas.GetAtlasTextures().GetMainAtlas();

    if (!layout.isValid || !sourceTexture.IsValid() || sourceTexture->GetFormat() != TextureFormat::R8)
    {
        HYP_LOG(Rendering, Error, "Font atlas {} has no R8 main atlas or label glyphs to bake grid labels from", fontAtlas.GetName());

        return Handle<Texture>::empty;
    }

    auto sourceReadScope = sourceTexture->GetReadScope();

    const ConstByteView sourcePixels = sourceTexture->GetImageData();
    const Vec2i sourceExtent = Vec2i(sourceTexture->GetExtent().GetXY());

    if (sourcePixels.Size() < size_t(sourceExtent.x) * size_t(sourceExtent.y))
    {
        HYP_LOG(Rendering, Error, "Font atlas {} has no image data to bake grid labels from", fontAtlas.GetName());

        return Handle<Texture>::empty;
    }

    ByteBuffer pixels;
    pixels.SetSize(size_t(layout.extent.x) * size_t(layout.extent.y));

    for (uint32 glyphIndex = 0; glyphIndex < NumGlyphs; glyphIndex++)
    {
        const Vec2i sourcePosition = layout.sourcePositions[glyphIndex];
        const Vec2i glyphSize = layout.sourceSizes[glyphIndex];

        if (glyphSize.x == 0)
        {
            continue;
        }

        const auto coverageAt = [&](int32 x, int32 y) -> float
        {
            if (x < 0 || y < 0 || x >= glyphSize.x || y >= glyphSize.y)
            {
                return 0.0f;
            }

            return float(sourcePixels.Data()[(sourcePosition.y + y) * sourceExtent.x + sourcePosition.x + x]) / 255.0f;
        };

        // texels the outline passes through: antialiased ones, or ones next to a texel on its other side.
        // each keeps how far inside (+) or outside (-) of the outline its center is
        Array<Vec2i> edgePositions;
        Array<float> edgeInsideDistances;

        for (int32 y = 0; y < glyphSize.y; y++)
        {
            for (int32 x = 0; x < glyphSize.x; x++)
            {
                const float coverage = coverageAt(x, y);
                const bool isInside = coverage >= 0.5f;

                const bool isEdge = (coverage > 0.02f && coverage < 0.98f)
                    || (coverageAt(x - 1, y) >= 0.5f) != isInside
                    || (coverageAt(x + 1, y) >= 0.5f) != isInside
                    || (coverageAt(x, y - 1) >= 0.5f) != isInside
                    || (coverageAt(x, y + 1) >= 0.5f) != isInside;

                if (isEdge)
                {
                    edgePositions.PushBack(Vec2i(x, y));
                    edgeInsideDistances.PushBack(MathUtil::Clamp(coverage - 0.5f, -0.5f, 0.5f));
                }
            }
        }

        const Vec2i cellSize = glyphSize + Vec2i(GlyphDistanceSpread * 2);

        for (int32 cellY = 0; cellY < cellSize.y; cellY++)
        {
            for (int32 cellX = 0; cellX < cellSize.x; cellX++)
            {
                const Vec2i position = Vec2i(cellX, cellY) - Vec2i(GlyphDistanceSpread);
                const bool isInside = coverageAt(position.x, position.y) >= 0.5f;

                float distance = float(GlyphDistanceSpread);

                for (size_t edgeIndex = 0; edgeIndex < edgePositions.Size(); edgeIndex++)
                {
                    const Vec2i delta = edgePositions[edgeIndex] - position;

                    if (MathUtil::Abs(delta.x) > GlyphDistanceSpread || MathUtil::Abs(delta.y) > GlyphDistanceSpread)
                    {
                        continue;
                    }

                    const float edgeInsideDistance = isInside ? edgeInsideDistances[edgeIndex] : -edgeInsideDistances[edgeIndex];

                    distance = MathUtil::Min(distance, Vec2f(delta).Length() + edgeInsideDistance);
                }

                const float signedDistance = isInside ? MathUtil::Max(distance, 0.0f) : -MathUtil::Max(distance, 0.0f);
                const float encoded = MathUtil::Clamp(0.5f + signedDistance / float(GlyphDistanceSpread * 2), 0.0f, 1.0f);

                pixels.Data()[size_t(cellY) * layout.extent.x + size_t(layout.cellOffsets[glyphIndex] + cellX)] = ubyte(encoded * 255.0f + 0.5f);
            }
        }
    }

    const TextureDesc textureDesc {
        TextureType::Texture2D,
        TextureFormat::R8,
        Vec3u { layout.extent.x, layout.extent.y, 1 },
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    };

    Handle<Texture> texture = MakeHandle<Texture>(textureDesc, pixels.ToByteView());
    texture->SetName(GetGlyphTextureName());

    return texture;
}

Name EditorGridLabelFont::GetGlyphTextureName()
{
    return NAME("EditorGridLabelGlyphs");
}

EditorGridLabelFont::EditorGridLabelFont() = default;

EditorGridLabelFont::~EditorGridLabelFont()
{
    if (m_loadTask.IsValid() && !m_loadTask.IsCompleted())
    {
        m_loadTask.Await();
    }
}

bool EditorGridLabelFont::Update()
{
    AssertOnThread(g_renderThread);

    m_isReady = false;

    if (!m_isLoaded)
    {
        // only loaded once - missing assets just leave the grid unlabeled
        if (!m_loadTask.IsValid())
        {
            m_loadTask = TaskSystem::GetInstance().Enqueue([]() -> LoadedGlyphs
                {
                    LoadedGlyphs loadedGlyphs;

                    const Handle<FontAtlas> fontAtlas = EngineDefaults::LoadAsset<FontAtlas>(EngineDefaults::Get().font);

                    Handle<Texture> texture;

#ifdef HYP_EDITOR
                    if (Handle<AssetRegistry> editorRegistry = GetEditorAssetRegistry(); editorRegistry.IsValid())
                    {
                        texture = TryCast<Texture>(editorRegistry->GetAsset(AssetBuckets::Textures, GetGlyphTextureName()));
                    }
#endif // HYP_EDITOR

                    if (!fontAtlas.IsValid() || !texture.IsValid())
                    {
                        HYP_LOG(Rendering, Warning, "Editor grid labels need the engine font and its baked glyphs, run the buildeditoricons commandlet");

                        return loadedGlyphs;
                    }

                    const LabelGlyphLayout layout = ComputeLabelGlyphLayout(*fontAtlas);

                    if (!layout.isValid || texture->GetExtent().GetXY() != layout.extent)
                    {
                        HYP_LOG(Rendering, Warning, "Editor grid label glyphs don't match the engine font, rerun the buildeditoricons commandlet");

                        return loadedGlyphs;
                    }

                    loadedGlyphs.glyphs = layout.glyphs;
                    loadedGlyphs.distanceRange = layout.distanceRange;
                    loadedGlyphs.texture = std::move(texture);

                    return loadedGlyphs;
                });

            return false;
        }

        if (!m_loadTask.IsCompleted())
        {
            return false;
        }

        m_loadedGlyphs = std::move(m_loadTask.Await());
        m_isLoaded = true;
    }

    Texture* glyphTexture = m_loadedGlyphs.texture.Get();

    if (!glyphTexture)
    {
        return false;
    }

    // uploaded lazily when first bound, same as ResourceBindings
    if (!glyphTexture->IsCreated())
    {
        Check(glyphTexture->Create());
    }

    m_isReady = glyphTexture->IsCreated();

    return m_isReady;
}

GpuImageView* EditorGridLabelFont::GetAtlasImageView() const
{
    if (!m_isReady)
    {
        return RI.placeholderData->GetImageView2D1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_loadedGlyphs.texture.Get());
}

#pragma endregion EditorGridLabelFont

#pragma region EditorGridPass

EditorGridPass::EditorGridPass()
    : FullScreenPass(FSP_EXTERNAL_RENDERTARGET)
{
    SetPassName(NAME("EditorGrid"));

    SetBlendFunction(BlendFunction::AlphaBlending());
}

EditorGridPass::~EditorGridPass()
{
}

void EditorGridPass::Create()
{
    AssertOnThread(g_renderThread);

    m_shaderDesc = ShaderDesc(NAME("EditorGrid"));

    FullScreenPass::Create();
}

void EditorGridPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.framebuffer);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    const bool drawLabels = m_labelFont.Update();

    EditorGridConstants constants {};

    constants.gridOffsetAndSize = Vec4f(
        g_cvEditorGridOffsetX.Get(),
        g_cvEditorGridOffsetY.Get(),
        g_cvEditorGridOffsetZ.Get(),
        MathUtil::Max(g_cvEditorGridSize.Get(), MathUtil::epsilonF));

    if (drawLabels)
    {
        constants.labelParams.x = 1.0f;
        constants.labelParams.y = m_labelFont.GetDistanceRange();
    }

    constants.glyphs = m_labelFont.GetGlyphs();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    ENGINE_STAT_GPU_SCOPE(&s_statDrawEditorGrid, &cr);

    cr << SetFillMode(FillMode::Fill);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(true);
    cr << SetStencilTest(false);
    cr << SetFaceCullMode(FaceCullMode::None);
    cr << SetTopology(Topology::Triangles);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetCurrentBlendFunction(GetBlendFunction());

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetCurrentFramebuffer(renderSetup.framebuffer);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetShaderUniform(0, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(1, "EditorGridConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(2, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(3, "GlyphAtlas"_sh, m_labelFont.GetAtlasImageView());

    RenderFullScreenQuad(frame, renderSetup);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    m_isFirstFrame = false;
}

#pragma endregion EditorGridPass

} // namespace Hyperion
