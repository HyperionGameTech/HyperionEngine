#include <rendering/RenderTextureMipmap.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(RenderTextureMipmapLevels)
    : RenderCommand
{
    GpuImageRef m_image;
    GpuImageViewRef m_imageView;
    Array<GpuImageViewRef> m_mipImageViews;
    Array<Handle<FullScreenPass>> m_passes;

    RENDER_COMMAND(RenderTextureMipmapLevels)(
        GpuImageRef image,
        GpuImageViewRef imageView,
        Array<GpuImageViewRef> mipImageViews,
        Array<Handle<FullScreenPass>> passes)
        : m_image(std::move(image)),
          m_imageView(std::move(imageView)),
          m_mipImageViews(std::move(mipImageViews)),
          m_passes(std::move(passes))
    {
        Assert(m_image != nullptr);
        Assert(m_imageView != nullptr);

        Assert(m_passes.Size() == m_mipImageViews.Size());

        for (SizeType index = 0; index < m_mipImageViews.Size(); index++)
        {
            Assert(m_mipImageViews[index] != nullptr);
            Assert(m_passes[index] != nullptr);
        }
    }

    virtual RendererResult operator()() override
    {
        // draw a quad for each level
        FrameBase* frame = g_renderBackend->GetCurrentFrame();
        RenderQueue& renderQueue = frame->renderQueue;

        const Vec3u extent = m_image->GetExtent();

        uint32 mipWidth = extent.x,
               mipHeight = extent.y;

        GpuImageRef& dstImage = m_image;

        for (uint32 mipLevel = 0; mipLevel < uint32(m_mipImageViews.Size()); mipLevel++)
        {
            Handle<FullScreenPass>& pass = m_passes[mipLevel];
            Assert(pass != nullptr);

            const uint32 prevMipWidth = mipWidth,
                         prevMipHeight = mipHeight;

            mipWidth = MathUtil::Max(1u, extent.x >> (mipLevel));
            mipHeight = MathUtil::Max(1u, extent.y >> (mipLevel));

            {
                pass->Begin(frame, NullRenderSetup());

                renderQueue << BindDescriptorTable(
                    pass->GetGraphicsPipeline()->GetDescriptorTable(),
                    pass->GetGraphicsPipeline(),
                    {},
                    frame->GetFrameIndex());

                frame->renderQueue << BindVertexBuffer(pass->GetQuadMesh()->GetVertexBuffer());
                frame->renderQueue << BindIndexBuffer(pass->GetQuadMesh()->GetIndexBuffer());
                frame->renderQueue << DrawIndexed(pass->GetQuadMesh()->NumIndices());

                pass->End(frame, NullRenderSetup());
            }

            const GpuImageRef& srcImage = pass->GetAttachment(0)->GetImage();

            // Blit into mip level
            renderQueue << InsertBarrier(dstImage, RS_COPY_DST, ImageSubResource { .baseMipLevel = mipLevel });
            renderQueue << InsertBarrier(srcImage, RS_COPY_SRC, ImageSubResource { .baseMipLevel = mipLevel });

            renderQueue << BlitRect(
                srcImage,
                dstImage,
                Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
                Rect<uint32> { 0, 0, dstImage->GetExtent().x, dstImage->GetExtent().y });

            renderQueue << InsertBarrier(srcImage, RS_SHADER_RESOURCE, ImageSubResource { .baseMipLevel = mipLevel });
            renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE, ImageSubResource { .baseMipLevel = mipLevel });
        }

        // all mip levels have been transitioned into this state
        renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);

        return {};
    }
};

#pragma endregion Render commands

#pragma region TextureMipmapRenderer

void TextureMipmapRenderer::RenderMipmaps(const Handle<Texture>& texture)
{
    Assert(texture.IsValid());

    GpuImageViewRef textureImageView = g_renderBackend->GetTextureImageView(texture);
    Assert(textureImageView.IsValid());

    const uint32 numMipLevels = texture->GetTextureDesc().NumMipmaps();
    const Vec3u extent = texture->GetExtent();

    Array<GpuImageViewRef> mipImageViews;
    mipImageViews.Resize(numMipLevels);

    Array<Handle<FullScreenPass>> passes;
    passes.Resize(numMipLevels);

    ShaderRef shader = g_shaderManager->GetOrCreate(
        NAME("GenerateMipmaps"),
        ShaderProperties());

    for (uint32 mipLevel = 0; mipLevel < numMipLevels; mipLevel++)
    {
        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        const uint32 mipWidth = MathUtil::Max(1u, extent.x >> mipLevel);
        const uint32 mipHeight = MathUtil::Max(1u, extent.y >> mipLevel);

        GpuImageViewRef mipImageView = g_renderBackend->MakeImageView(texture->GetGpuImage(), mipLevel, 1, 0, texture->NumFaces());
        DeferCreate(mipImageView);

        const DescriptorSetRef& generateMipmapsDescriptorSet = descriptorTable->GetDescriptorSet(NAME("GenerateMipmapsDescriptorSet"), 0);
        Assert(generateMipmapsDescriptorSet != nullptr);
        generateMipmapsDescriptorSet->SetElement(NAME("InputTexture"), mipLevel == 0 ? textureImageView : mipImageViews[mipLevel - 1]);
        DeferCreate(descriptorTable);

        mipImageViews[mipLevel] = std::move(mipImageView);

        {
            Handle<FullScreenPass> pass = CreateObject<FullScreenPass>(
                shader,
                descriptorTable,
                texture->GetFormat(),
                Vec2u { mipWidth, mipHeight },
                nullptr);

            pass->Create();

            passes[mipLevel] = std::move(pass);
        }
    }

    PUSH_RENDER_COMMAND(
        RenderTextureMipmapLevels,
        texture->GetGpuImage(),
        textureImageView,
        mipImageViews,
        passes);

    passes.Clear();

    SafeRelease(std::move(mipImageViews));
}

#pragma endregion TextureMipmapRenderer

} // namespace hyperion
