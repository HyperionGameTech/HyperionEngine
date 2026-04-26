/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Texture.hpp>
#include <rendering/CBufferAllocator.hpp>
#include <rendering/StructuredBufferAllocator.hpp>
#include <rendering/SamplerCache.hpp>

#include <rendering/renderers/SpriteRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/View.hpp>
#include <scene/Sprite.hpp>
#include <scene/TextSprite.hpp>
#include <scene/camera/Camera.hpp> // Needed for .Get()

#include <ui/font/FontAtlas.hpp>

#include <asset/AssetRegistry.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/CVarManager.hpp>
#include <engine/EngineGlobals.hpp>

#include <SpriteRenderer.generated.inl>

namespace Hyperion {

struct SpriteInstanceData
{
    Vec4f positionSize;
    Vec4f color;
    Vec4u flags; // x = alwaysFaceCamera
};

struct alignas(16) TextSpriteInstanceData
{
    Mat4f transform;
    Vec2f texcoordStart;
    Vec2f texcoordEnd;
    uint32 textureIndex;
    uint32 colorPacked;
};

struct FontAtlasCharacterIterator
{
    Vec2f placement;
    Vec2f atlasPixelSize;
    Vec2f cellDimensions;

    Vec2i charOffset;

    Vec2f glyphDimensions;
    Vec2f glyphScaling;

    float bearingY;
    float charWidth;
};

struct TextMetrics
{
    Vec2f size;
};

static TextMetrics MeasureText(
    const FontAtlas& fontAtlas,
    const String& text,
    float textSize)
{
    HYP_SCOPE;

    Vec2f placement = Vec2f::Zero();
    Vec2f totalSize = Vec2f::Zero();

    const size_t length = text.Length();
    const Vec2f cellDimensions = Vec2f(fontAtlas.GetCellDimensions()) / 64.0f;

    for (size_t i = 0; i < length; i++)
    {
        const utf::Char32 ch = text.GetChar(i);

        if (ch == utf::Char32(' '))
        {
            placement.x += cellDimensions.x * 0.5f;
            continue;
        }

        if (ch == utf::Char32('\n'))
        {
            totalSize.x = MathUtil::Max(totalSize.x, placement.x);
            placement.x = 0.0f;
            placement.y += cellDimensions.y;
            continue;
        }

        Optional<const GlyphMetrics&> glyphMetrics = fontAtlas.GetGlyphMetricsForChar(ch);
        if (!glyphMetrics.HasValue() || (glyphMetrics->width == 0 || glyphMetrics->height == 0))
        {
            continue;
        }

        const Vec2f glyphDimensions = Vec2f(float(glyphMetrics->width), float(glyphMetrics->height)) / 64.0f;
        const float charWidth = float(glyphMetrics->advance / 64) / 64.0f;
        const float bearingY = float(glyphMetrics->height - glyphMetrics->bearingY) / 64.0f;

        const float charHeight = glyphDimensions.y + bearingY;
        totalSize.y = MathUtil::Max(totalSize.y, placement.y + charHeight);

        placement.x += charWidth;
    }

    totalSize.x = MathUtil::Max(totalSize.x, placement.x);

    return { totalSize * textSize };
}

template <class Callback>
static void ForEachCharacter(
    const FontAtlas& fontAtlas,
    const String& text,
    float textSize,
    Callback&& callback)
{
    HYP_SCOPE;

    Vec2f placement = Vec2f::Zero();

    const size_t length = text.Length();

    const Vec2f cellDimensions = Vec2f(fontAtlas.GetCellDimensions()) / 64.0f;

    const Handle<Texture>& mainTextureAtlas = fontAtlas.GetAtlasTextures().GetMainAtlas();
    AssertDebug(mainTextureAtlas.IsValid(), "Main texture atlas is invalid");

    if (!mainTextureAtlas.IsValid())
    {
        return;
    }

    Vec2f atlasPixelSize;

    if (mainTextureAtlas->GetExtent().Volume() != 0)
    {
        atlasPixelSize = Vec2f::One() / Vec2f(mainTextureAtlas->GetExtent().GetXY());
    }

    Array<FontAtlasCharacterIterator> currentWordChars;

    const auto iterateCurrentWord = [&currentWordChars, &callback]()
    {
        for (const FontAtlasCharacterIterator& characterIterator : currentWordChars)
        {
            callback(characterIterator);
        }

        currentWordChars.Clear();
    };

    for (size_t i = 0; i < length; i++)
    {
        const utf::Char32 ch = text.GetChar(i);

        if (ch == utf::Char32(' '))
        {
            placement.x += cellDimensions.x * 0.5f;
            iterateCurrentWord();
            continue;
        }

        if (ch == utf::Char32('\n'))
        {
            placement.x = 0.0f;
            placement.y += cellDimensions.y;
            iterateCurrentWord();
            continue;
        }

        Optional<const GlyphMetrics&> glyphMetrics = fontAtlas.GetGlyphMetricsForChar(ch);

        if (!glyphMetrics.HasValue() || (glyphMetrics->width == 0 || glyphMetrics->height == 0))
        {
            HYP_LOG_ONCE(Rendering, Warning, "Ensure how to render char: {}", (int)ch);
            continue;
        }

        FontAtlasCharacterIterator& characterIterator = currentWordChars.EmplaceBack();
        characterIterator.placement = placement;
        characterIterator.atlasPixelSize = atlasPixelSize;
        characterIterator.cellDimensions = cellDimensions;
        characterIterator.charOffset = glyphMetrics->imagePosition;
        characterIterator.glyphDimensions = Vec2f(float(glyphMetrics->width), float(glyphMetrics->height)) / 64.0f;
        characterIterator.bearingY = float(glyphMetrics->height - glyphMetrics->bearingY) / 64.0f;
        characterIterator.charWidth = float(glyphMetrics->advance / 64) / 64.0f;

        placement.x += characterIterator.charWidth;
    }

    iterateCurrentWord();
}

#pragma region SpriteRenderer

SpriteRenderer::SpriteRenderer()
{
}

void SpriteRenderer::Initialize()
{
    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetName(NAME("SpriteMesh"));
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->SetIsTransient(true);

    {
        VertexArrayView vd = m_quadMesh->GetVertexData();
        ByteView id = m_quadMesh->GetIndexData();

        Array<SimpleVertex> newVertices;
        newVertices.Resize(vd.vertexCount);
        Memory::Copy(newVertices.Data(), vd.floatData, vd.vertexCount * sizeof(SimpleVertex));

        for (SimpleVertex& vert : newVertices)
        {
            vert.posX = (vert.posX + 1.0f) * 0.5f;
            vert.posY = (vert.posY + 1.0f) * 0.5f;
        }

        VertexArrayView vertexArrayView {};
        vertexArrayView.floatData = reinterpret_cast<const float*>(newVertices.Data());
        vertexArrayView.vertexCount = newVertices.Size();
        vertexArrayView.layoutDesc = { VT_Simple };

        Array<ubyte> indexData;
        indexData.Resize(id.Size());
        Memory::Copy(indexData.Data(), id.Data(), id.Size());

        m_quadMesh->SetMeshData(m_quadMesh->GetMeshDesc(), vertexArrayView, indexData);
    }

    InitObject(m_quadMesh);

    GetEngineAssetRegistry()->PutAsset(m_quadMesh);
}

void SpriteRenderer::Shutdown()
{
    EnqueueDeletion(std::move(m_quadMesh));
}

void SpriteRenderer::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(renderSetup.view != nullptr);

    SpriteRendererPassData* pd = DynamicCast<SpriteRendererPassData>(FetchViewPassData(renderSetup.view));
    AssertDebug(pd != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (!rpl.GetSprites().NumCurrent())
    {
        return;
    }

    Mesh* quadMesh = m_quadMesh;

    if (!quadMesh)
    {
        return;
    }

    uint32 numSprites = 0;
    uint32 numTextCharacters = 0;
    for (Sprite* sprite : rpl.GetSprites())
    {
        if (!sprite)
        {
            continue;
        }

        if (sprite->IsA<TextSprite>())
        {
            RenderProxySprite* spriteProxy = rpl.GetSprites().GetProxy(sprite->Id());
            if (spriteProxy && !spriteProxy->text.Empty())
            {
                for (size_t i = 0; i < spriteProxy->text.Length(); i++)
                {
                    utf::Char32 ch = spriteProxy->text.GetChar(i);
                    if (ch != utf::Char32(' ') && ch != utf::Char32('\n'))
                    {
                        numTextCharacters++;
                    }
                }
            }
        }
        else
        {
            numSprites++;
        }
    }

    if (numSprites == 0 && numTextCharacters == 0)
    {
        return;
    }

    CommandRecorder& cr = frame->cr;
    View* view = renderSetup.view;

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera().Get()));
    Assert(cameraProxy != nullptr);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    g_renderInterface->cbufferAllocator->Write(&cameraProxy->bufferData);
    g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetTopology(Topology::TOP_TRIANGLES);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    if (numSprites > 0)
    {
        StructuredBuffer& instanceBuffer = g_renderInterface->sbufferAllocator->AcquireBuffer(numSprites, sizeof(SpriteInstanceData));

        size_t offset = 0;

        for (Sprite* sprite : rpl.GetSprites())
        {
            if (!sprite || sprite->IsA<TextSprite>())
            {
                continue;
            }

            RenderProxySprite* spriteProxy = rpl.GetSprites().GetProxy(sprite->Id());
            if (!spriteProxy)
            {
                continue;
            }

            SpriteInstanceData data {};
            data.positionSize = Vec4f(sprite->GetWorldTranslation(), sprite->size);
            data.color = Vec4f(sprite->color);
            data.flags = Vec4u(spriteProxy->bufferData.alwaysFaceCamera, 0u, 0u, 0u);

            instanceBuffer.Write(offset, sizeof(SpriteInstanceData), &data);
            offset += sizeof(SpriteInstanceData);
        }

        instanceBuffer.Flush();

        ShaderDesc shaderDesc;
        shaderDesc.name = NAME("Sprite");

        cr << SetCurrentShader(shaderDesc);
        
        cr << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(1, "SpriteInstanceBuffer"_sh, instanceBuffer.gpuBuffer);
        cr << SetShaderUniform(2, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << SetCurrentBlendFunction(BlendFunction::AlphaBlending());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(false);
        cr << SetFaceCullMode(FaceCullMode::FCM_NONE);

        cr << CommitDrawState();

        cr << BindVertexBuffer(quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(quadMesh->GetIndexBuffer());

        cr << DrawIndexed(quadMesh->NumIndices(), numSprites);
    }

    if (numTextCharacters > 0)
    {
        Array<TextSpriteInstanceData> charData;
        charData.Reserve(numTextCharacters);

        for (Sprite* sprite : rpl.GetSprites())
        {
            if (!sprite || !sprite->IsA<TextSprite>())
            {
                continue;
            }

            TextSprite* textSprite = static_cast<TextSprite*>(sprite);

            RenderProxySprite* spriteProxy = rpl.GetSprites().GetProxy(textSprite->Id());
            if (!spriteProxy || !spriteProxy->fontAtlas)
            {
                continue;
            }

            const Vec3f worldPos = sprite->GetWorldTranslation(); // spriteProxy->bufferData.positionSize.GetXYZ();
            const float textSize = spriteProxy->bufferData.positionSize.w;

            const uint32 textureIndex = spriteProxy->texture ? spriteProxy->texture->Id().ToIndex() : uint32(-1);

            const TextMetrics metrics = MeasureText(*spriteProxy->fontAtlas, spriteProxy->text, textSize);

            ForEachCharacter(*spriteProxy->fontAtlas, spriteProxy->text, textSize, [&](const FontAtlasCharacterIterator& iter)
                {
                    const float scaleX = iter.glyphDimensions.x * textSize;
                    const float scaleY = iter.glyphDimensions.y * textSize;

                    const float offsetY = -iter.bearingY * textSize;
                    const Vec3f charTranslation(
                        worldPos.x + iter.placement.x * textSize - metrics.size.x * 0.5f,
                        worldPos.y + iter.placement.y * textSize + offsetY - metrics.size.y * 0.5f,
                        worldPos.z
                    );

                    TextSpriteInstanceData data {};

                    Transform t;
                    t.SetScale(Vec3f(scaleX, scaleY, 0.1f));
                    t.SetTranslation(charTranslation);

                    data.transform = t.GetMatrix();

                    data.textureIndex = textureIndex;
                    data.colorPacked = spriteProxy->textColor.Packed();

                    Vec2f atlasPixelSize;
                    if (spriteProxy->texture && spriteProxy->texture->GetExtent().Volume() != 0)
                    {
                        atlasPixelSize = Vec2f::One() / Vec2f(spriteProxy->texture->GetExtent().GetXY());
                    }

                    Vec2f charOffsetF = Vec2f(iter.charOffset);
                    data.texcoordStart = Vec2f(charOffsetF * atlasPixelSize);
                    data.texcoordEnd = Vec2f((charOffsetF + (iter.glyphDimensions * 64.0f)) * atlasPixelSize);

                    charData.PushBack(data);
                });
        }

        StructuredBuffer& textInstanceBuffer = g_renderInterface->sbufferAllocator->AcquireBuffer(charData.Size(), sizeof(TextSpriteInstanceData));

        size_t textOffset = 0;
        for (const auto& data : charData)
        {
            textInstanceBuffer.Write(textOffset, sizeof(TextSpriteInstanceData), &data);
            textOffset += sizeof(TextSpriteInstanceData);
        }

        textInstanceBuffer.Flush();

        static Sampler* s_textSpriteSampler = g_renderInterface->samplerCache->GetOrCreate(SamplerDesc {
            TFM_LINEAR,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE,
            SamplerCompareOp::None
        });

        ShaderDesc textShaderDesc;
        textShaderDesc.name = NAME("TextSprite");

        cr << SetCurrentShader(textShaderDesc);

        cr << SetShaderUniform(0, "SamplerLinear"_sh, s_textSpriteSampler);

        cr << SetShaderUniform(1, "TextSpriteInstanceBuffer"_sh, textInstanceBuffer.gpuBuffer);
        cr << SetShaderUniform(2, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << SetCurrentBlendFunction(BlendFunction::AlphaBlending());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(false);
        cr << SetFaceCullMode(FaceCullMode::FCM_NONE);

        cr << CommitDrawState();

        cr << BindVertexBuffer(quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(quadMesh->GetIndexBuffer());

        cr << DrawIndexed(quadMesh->NumIndices(), charData.Size());
    }
}

PassData* SpriteRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    SpriteRendererPassData* pd = new SpriteRendererPassData();

    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion SpriteRenderer

} // namespace Hyperion