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

#include <rendering/renderers/SpriteRenderer.hpp>

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

struct TextSpriteInstanceData
{
    Mat4f transform;
    Vec4f color;
    Vec4f texcoordStart;
    Vec4f texcoordEnd;
};

struct FontAtlasCharacterIterator
{
    Vec2f placement;
    Vec2f atlasPixelSize;
    Vec2f cellDimensions;

    Vec2i charOffset;

    Vec2f glyphDimensions;

    float bearingY;
    float charWidth;
};

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
    m_quadMesh = MeshBuilder::Cube(); // Quad();
    m_quadMesh->SetName(NAME("SpriteMesh"));
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->SetIsTransient(true);
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
    uint32 numTextSprites = 0;
    for (Sprite* sprite : rpl.GetSprites())
    {
        if (!sprite)
        {
            continue;
        }

        if (sprite->IsA<TextSprite>())
        {
            numTextSprites++;
        }
        else
        {
            numSprites++;
        }
    }

    if (numSprites == 0 && numTextSprites == 0)
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

        cr << SetShaderUniform(0, "SpriteInstanceBuffer"_sh, instanceBuffer.gpuBuffer);
        cr << SetShaderUniform(1, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << SetCurrentBlendFunction(BlendFunction::AlphaBlending());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(false);
        cr << SetFaceCullMode(FaceCullMode::FCM_NONE);

        cr << CommitDrawState();

        cr << BindVertexBuffer(quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(quadMesh->GetIndexBuffer());

        cr << DrawIndexed(quadMesh->NumIndices(), numSprites);
    }

    if (numTextSprites > 0)
    {
        StructuredBuffer& textInstanceBuffer = g_renderInterface->sbufferAllocator->AcquireBuffer(numTextSprites, sizeof(TextSpriteInstanceData));

        size_t textOffset = 0;

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

            const Vec3f worldPos = textSprite->GetWorldTranslation();
            const float textSize = textSprite->GetTextSize();

            ForEachCharacter(*spriteProxy->fontAtlas, spriteProxy->text, textSize, [&](const FontAtlasCharacterIterator& iter)
                {
                    Transform characterTransform;
                    characterTransform.scale = Vec3f(iter.glyphDimensions.x * textSize, iter.glyphDimensions.y * textSize, 0.01f);
                    characterTransform.translation.y += (iter.cellDimensions.y - iter.glyphDimensions.y) * textSize;
                    characterTransform.translation.y += iter.bearingY * textSize;
                    characterTransform.translation += Vec3f(iter.placement.x * textSize, iter.placement.y * textSize, 0.0f);
                    characterTransform.translation += worldPos;

                    Mat4f finalTransform = Mat4f::Translation(characterTransform.translation) * Mat4f::Scaling(characterTransform.scale);

                    TextSpriteInstanceData data {};
                    data.transform = finalTransform;
                    data.color = Vec4f(spriteProxy->textColor);

                    Vec2f atlasPixelSize;
                    if (spriteProxy->texture && spriteProxy->texture->GetExtent().Volume() != 0)
                    {
                        atlasPixelSize = Vec2f::One() / Vec2f(spriteProxy->texture->GetExtent().GetXY());
                    }

                    Vec2f charOffsetF = Vec2f(iter.charOffset);
                    data.texcoordStart = Vec4f(charOffsetF * atlasPixelSize, 0.0f, 0.0f);
                    data.texcoordEnd = Vec4f((charOffsetF + (iter.glyphDimensions * 64.0f)) * atlasPixelSize, 0.0f, 0.0f);

                    textInstanceBuffer.Write(textOffset, sizeof(TextSpriteInstanceData), &data);
                    textOffset += sizeof(TextSpriteInstanceData);
                });
        }

        textInstanceBuffer.Flush();

        ShaderDesc textShaderDesc;
        textShaderDesc.name = NAME("TextSprite");

        cr << SetCurrentShader(textShaderDesc);

        cr << SetShaderUniform(0, "TextSpriteInstanceBuffer"_sh, textInstanceBuffer.gpuBuffer);
        cr << SetShaderUniform(1, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << SetCurrentBlendFunction(BlendFunction::AlphaBlending());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(false);
        cr << SetFaceCullMode(FaceCullMode::FCM_NONE);

        cr << CommitDrawState();

        cr << BindVertexBuffer(quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(quadMesh->GetIndexBuffer());

        cr << DrawIndexed(quadMesh->NumIndices(), numTextSprites);
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