/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialInstance.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RawBufferAllocator.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/TextureViewCache.hpp>

#include <Rendering/Passes/SpritePass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/View.hpp>
#include <Scene/Sprite.hpp>
#include <Scene/TextSprite.hpp>
#include <Scene/Camera/Camera.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/EngineGlobals.hpp>

#include <SpritePass.generated.inl>

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

template <class AllocatorType, class Callback>
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

    if (!mainTextureAtlas.IsValid())
    {
        return;
    }

    Vec2f atlasPixelSize;

    if (mainTextureAtlas->GetExtent().Volume() != 0)
    {
        atlasPixelSize = Vec2f::One() / Vec2f(mainTextureAtlas->GetExtent().GetXY());
    }

    Array<FontAtlasCharacterIterator, AllocatorType> currentWordChars;
    currentWordChars.Reserve(length);

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

#pragma region SpritePass

SpritePass::SpritePass()
{
}

void SpritePass::Initialize()
{
    CreateQuadMeshes();
}

void SpritePass::Shutdown()
{
    EnqueueDeletion(std::move(m_quadMesh));
    EnqueueDeletion(std::move(m_textQuadFrontMesh));
    EnqueueDeletion(std::move(m_textQuadBackMesh));
}

void SpritePass::CreateQuadMeshes()
{
    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetName(NAME("SpriteMesh"));

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

        m_quadMesh->SetIsTransient(true);
        m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        m_quadMesh->UploadGpuData();
    }

    Handle<Mesh> textQuadMesh = MeshBuilder::Quad();
    textQuadMesh->SetName(NAME("TextSpriteQuad"));
    textQuadMesh->SetPersistentRequested(true);

    Transform backTransform;
    backTransform.rotation = Quat4f::AxisAngles(Vec3f::UnitY(), MathUtil::DegToRad(180.0f));
    backTransform.scale = Vec3f(1.0f, 1.0f, -1.0f);

    m_textQuadBackMesh = MeshBuilder::ApplyTransform(textQuadMesh, backTransform);
    m_textQuadBackMesh->SetName(NAME("TextSpriteBack"));
    m_textQuadBackMesh->SetIsTransient(true);
    m_textQuadBackMesh->SetFlags(MeshFlags::ViewIndependent);

    Transform frontTransform;

    m_textQuadFrontMesh = MeshBuilder::ApplyTransform(textQuadMesh, frontTransform);
    m_textQuadFrontMesh->SetName(NAME("TextSpriteFront"));
    m_textQuadFrontMesh->SetIsTransient(true);
    m_textQuadFrontMesh->SetFlags(MeshFlags::ViewIndependent);

    Handle<Mesh> textQuadMeshes[2] = { m_textQuadFrontMesh, m_textQuadBackMesh };

    for (Handle<Mesh>& mesh : textQuadMeshes)
    {
        auto readScope = mesh->GetReadScope();

        MeshDesc desc = mesh->GetMeshDesc();

        VertexArrayView vd = mesh->GetVertexData();
        Array<ubyte> indexData = mesh->GetIndexData();

        Array<float> newVertices;
        newVertices.Resize(vd.vertexCount * (vd.layoutDesc.VertexSize() / sizeof(float)));
        Memory::Copy(newVertices.Data(), vd.floatData, vd.vertexCount * vd.layoutDesc.VertexSize());

        for (float* f = newVertices.Begin(); f < newVertices.End(); f += vd.layoutDesc.VertexSize() / sizeof(float))
        {
            *f = (*f + 1.0f) * 0.5f;
            *(f + 1) = (*(f + 1) + 1.0f) * 0.5f;
        }

        VertexArrayView vertexArrayView = vd;
        vertexArrayView.floatData = reinterpret_cast<const float*>(newVertices.Data());

        readScope.Reset();

        mesh->SetMeshData(desc, vertexArrayView, indexData);

        mesh->UploadGpuData();
    }
}

void SpritePass::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    Assert(renderSetup.view != nullptr);

    SpritePassData* pd = DynamicCast<SpritePassData>(FetchViewPassData(renderSetup.view));
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

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));
    Assert(cameraProxy != nullptr);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    static const IRenderConfig& s_renderConfig = RI.GetRenderConfig();
    static const bool s_isBindlessSupported = s_renderConfig.bindlessTextures;

    cr << SetCurrentViewport(renderSetup.viewport);
    cr << SetTopology(Topology::TOP_TRIANGLES);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    Sampler* sampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        SamplerCompareOp::None
    });

    size_t numToDraw = 0;

    auto FlushDraws = [&](const StructuredBuffer& instanceBuffer)
    {
        if (numToDraw == 0)
        {
            return;
        }

        AssertDebug(instanceBuffer.gpuBuffer->Size() >= numToDraw * sizeof(SpriteInstanceData));

        ShaderDesc shaderDesc;
        shaderDesc.name = NAME("Sprite");

        cr << SetCurrentShader(shaderDesc);

        cr << SetShaderUniform(0, "SamplerLinear"_sh, sampler);
        cr << SetShaderUniform(1, "SpriteInstanceBuffer"_sh, instanceBuffer);
        cr << SetShaderUniform(2, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << SetCurrentBlendFunction(BlendFunction::AlphaBlending());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
        cr << SetFaceCullMode(FCM_BACK);

        cr << CommitDrawState();

        cr << BindVertexBuffer(quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(quadMesh->GetIndexBuffer());

        cr << DrawIndexed(quadMesh->NumIndices(), uint32(numToDraw));

        numToDraw = 0;
    };

    auto FlushDrawsText = [&](
        Texture* fontTexture,
        Span<TextSpriteInstanceData> charDataFront,
        Span<TextSpriteInstanceData> charDataBack)
    {
        if (numToDraw == 0)
        {
            return;
        }

        StructuredBuffer& textInstanceBufferFront = RI.bufferAllocator->AcquireStructuredBuffer(charDataFront.Size(), sizeof(TextSpriteInstanceData));

        size_t textOffset = 0;
        for (const auto& data : charDataFront)
        {
            textInstanceBufferFront.Write(textOffset, sizeof(TextSpriteInstanceData), &data);
            textOffset += sizeof(TextSpriteInstanceData);
        }

        textInstanceBufferFront.FlushBatched();

        StructuredBuffer& textInstanceBufferBack = RI.bufferAllocator->AcquireStructuredBuffer(charDataBack.Size(), sizeof(TextSpriteInstanceData));

        textOffset = 0;
        for (const auto& data : charDataBack)
        {
            textInstanceBufferBack.Write(textOffset, sizeof(TextSpriteInstanceData), &data);
            textOffset += sizeof(TextSpriteInstanceData);
        }

        textInstanceBufferBack.FlushBatched();

        ShaderDesc textShaderDesc;
        textShaderDesc.name = NAME("TextSprite");

        cr << SetCurrentShader(textShaderDesc);

        cr << SetCurrentBlendFunction(BlendFunction::AlphaBlending());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
        cr << SetFaceCullMode(FCM_BACK);

        cr << SetShaderUniform(0, "SamplerLinear"_sh, sampler);

        cr << SetShaderUniform(2, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        if (!s_isBindlessSupported)
        {
            cr << SetShaderUniform(3, "FontTexture"_sh, RI.textureViewCache->GetOrCreate(fontTexture));
        }

        // Draw front face
        cr << SetShaderUniform(1, "TextSpriteInstanceBuffer"_sh, textInstanceBufferFront);
        cr << CommitDrawState();

        cr << BindVertexBuffer(m_textQuadFrontMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_textQuadFrontMesh->GetIndexBuffer());
        cr << DrawIndexed(m_textQuadFrontMesh->NumIndices(), charDataFront.Size());

        // Draw back face
        cr << SetShaderUniform(1, "TextSpriteInstanceBuffer"_sh, textInstanceBufferBack);
        cr << CommitDrawState();

        cr << BindVertexBuffer(m_textQuadBackMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_textQuadBackMesh->GetIndexBuffer());
        cr << DrawIndexed(m_textQuadBackMesh->NumIndices(), charDataBack.Size());

        // reset
        cr << SetFaceCullMode(FCM_BACK);

        numToDraw = 0;
    };

    if (numSprites > 0)
    {
        StructuredBuffer& instanceBuffer = RI.bufferAllocator->AcquireStructuredBuffer(numSprites, sizeof(SpriteInstanceData));

        size_t offset = 0;

        Texture* lastTexture = nullptr;

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

            if (!s_isBindlessSupported && spriteProxy->texture != lastTexture && numToDraw != 0)
            {
                FlushDraws(instanceBuffer);
            }

            lastTexture = spriteProxy->texture;
            ++numToDraw;
        }

        instanceBuffer.FlushBatched();

        FlushDraws(instanceBuffer);
    }

    if (numTextCharacters > 0)
    {
        Array<TextSpriteInstanceData, RenderTempAllocator> charDataFront;
        charDataFront.Reserve(numTextCharacters);

        Array<TextSpriteInstanceData, RenderTempAllocator> charDataBack;
        charDataBack.Reserve(numTextCharacters);

        Texture* lastTexture = nullptr;

        for (Sprite* sprite : rpl.GetSprites())
        {
            if (!sprite || !sprite->IsA<TextSprite>())
            {
                continue;
            }

            TextSprite* textSprite = StaticCast<TextSprite>(sprite);

            RenderProxySprite* spriteProxy = rpl.GetSprites().GetProxy(textSprite->Id());
            if (!spriteProxy || !spriteProxy->fontAtlas)
            {
                continue;
            }

            const Quat4f worldRot = sprite->GetWorldRotation();
            const Vec3f worldPos = sprite->GetWorldTranslation();

            const float textSize = spriteProxy->bufferData.positionSize.w;

            const uint32 textureIndex = spriteProxy->texture ? spriteProxy->texture->Id().ToIndex() : uint32(-1);

            const TextMetrics metrics = MeasureText(*spriteProxy->fontAtlas, spriteProxy->text, textSize);

            ForEachCharacter<RenderTempAllocator>(*spriteProxy->fontAtlas, spriteProxy->text, textSize, [&](const FontAtlasCharacterIterator& iter)
                {
                    const float scaleX = iter.glyphDimensions.x * textSize;
                    const float scaleY = iter.glyphDimensions.y * textSize;

                    const float offsetY = -iter.bearingY * textSize;

                    Vec2f atlasPixelSize;
                    if (spriteProxy->texture && spriteProxy->texture->GetExtent().Volume() != 0)
                    {
                        atlasPixelSize = Vec2f::One() / Vec2f(spriteProxy->texture->GetExtent().GetXY());
                    }

                    Vec2f charOffsetF = Vec2f(iter.charOffset);

                    const uint32 packedColor = spriteProxy->textColor.Packed();

                    // Front face
                    {
                        const Vec3f charTranslation = worldPos + worldRot.Inverse().RotateVector(Vec3f(
                            iter.placement.x * textSize - metrics.size.x * 0.5f,
                            iter.placement.y * textSize + offsetY - metrics.size.y * 0.5f,
                            0.0f
                        ));

                        Transform t;
                        t.SetScale(Vec3f(scaleX, scaleY, 0.1f));
                        t.SetTranslation(charTranslation);
                        t.SetRotation(worldRot);

                        TextSpriteInstanceData data {};
                        data.transform = t.GetMatrix();
                        data.textureIndex = textureIndex;
                        data.colorPacked = packedColor;
                        data.texcoordStart = Vec2f(charOffsetF * atlasPixelSize);
                        data.texcoordEnd = Vec2f((charOffsetF + (iter.glyphDimensions * 64.0f)) * atlasPixelSize);

                        charDataFront.PushBack(data);
                    }

                    // Back face - mirror X around text center so glyph order reads correctly from behind.
                    // We mirror the character center, not the left edge, to handle variable-width characters.
                    {
                        const Vec3f charTranslation = worldPos + worldRot.Inverse().RotateVector(Vec3f(
                            -iter.placement.x * textSize + metrics.size.x * 0.5f - scaleX,
                            iter.placement.y * textSize + offsetY - metrics.size.y * 0.5f,
                            0.0f
                        ));

                        Transform t;
                        t.SetScale(Vec3f(scaleX, scaleY, 0.1f));
                        t.SetTranslation(charTranslation);
                        t.SetRotation(worldRot);

                        TextSpriteInstanceData data {};
                        data.transform = t.GetMatrix();
                        data.textureIndex = textureIndex;
                        data.colorPacked = packedColor;
                        data.texcoordStart = Vec2f(charOffsetF * atlasPixelSize);
                        data.texcoordEnd = Vec2f((charOffsetF + (iter.glyphDimensions * 64.0f)) * atlasPixelSize);

                        charDataBack.PushBack(data);
                    }
                });

            if (!s_isBindlessSupported && lastTexture != spriteProxy->texture && numToDraw != 0)
            {
                FlushDrawsText(lastTexture, charDataFront, charDataBack);

                charDataBack.Resize(0);
                charDataFront.Resize(0);
            }

            lastTexture = spriteProxy->texture;
            ++numToDraw;
        }

        FlushDrawsText(lastTexture, charDataFront, charDataBack);
    }
}

PassData* SpritePass::CreateViewPassData(View* view, PassDataExt&)
{
    SpritePassData* pd = new SpritePassData();
    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion SpritePass

} // namespace Hyperion
