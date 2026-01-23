#include <RenderingPch.hpp>

#include <rendering/util/TextureMipmapRenderer.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/Frame.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RendererBase.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>

#include <engine/threads/RenderThread.hpp>

namespace Hyperion {

#pragma region Render commands

struct RenderTextureMipmapLevelsTask
{
    GpuImageRef image;
    GpuImageViewRef imageView;
    Array<GpuImageViewRef> mipImageViews;
    Array<FullScreenPass*> passes;

    RenderTextureMipmapLevelsTask(
        const GpuImageRef& image,
        const GpuImageViewRef& imageView,
        Array<GpuImageViewRef>&& mipImageViews,
        Array<FullScreenPass*>&& passes)
        : image(std::move(image)),
          imageView(std::move(imageView)),
          mipImageViews(std::move(mipImageViews)),
          passes(std::move(passes))
    {
        Assert(this->image != nullptr);
        Assert(this->imageView != nullptr);

        Assert(this->passes.Size() == this->mipImageViews.Size());

        for (SizeType index = 0; index < this->mipImageViews.Size(); index++)
        {
            Assert(this->mipImageViews[index] != nullptr);
            Assert(this->passes[index] != nullptr);
        }
    }

    ~RenderTextureMipmapLevelsTask()
    {
        for (FullScreenPass* pass : passes)
        {
            delete pass;
        }

        SafeDelete(std::move(mipImageViews));
    }

    void operator()()
    {
        // draw a quad for each level
        Frame* frame = g_renderBackend->GetCurrentFrame();
        RenderQueue& rq = frame->renderQueue;

        const Vec3u extent = image->GetExtent();

        uint32 mipWidth = extent.x,
               mipHeight = extent.y;

        GpuImage* dstImage = image;

        for (uint32 mipLevel = 0; mipLevel < uint32(mipImageViews.Size()); mipLevel++)
        {
            FullScreenPass* pass = passes[mipLevel];
            Assert(pass != nullptr);

            const uint32 prevMipWidth = mipWidth,
                         prevMipHeight = mipHeight;

            mipWidth = MathUtil::Max(1u, extent.x >> (mipLevel));
            mipHeight = MathUtil::Max(1u, extent.y >> (mipLevel));

            {
                pass->Begin(frame, NullRenderSetup());

                rq << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
                rq << SetShaderUniform(1, "InTexture"_sh, mipLevel == 0 ? imageView : mipImageViews[mipLevel - 1]);

                pass->RenderFullScreenQuad(frame, NullRenderSetup());

                pass->End(frame, NullRenderSetup());
            }

            const GpuImageRef& srcImage = pass->GetAttachment(0)->GetImage();

            // Blit into mip level
            rq << InsertBarrier(dstImage, RS_COPY_DST, ImageSubResource { .baseMipLevel = mipLevel });
            rq << InsertBarrier(srcImage, RS_COPY_SRC, ImageSubResource { .baseMipLevel = mipLevel });

            rq << BlitRect(
                srcImage,
                dstImage,
                Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
                Rect<uint32> { 0, 0, dstImage->GetExtent().x, dstImage->GetExtent().y });

            rq << InsertBarrier(srcImage, RS_SHADER_RESOURCE, ImageSubResource { .baseMipLevel = mipLevel });
            rq << InsertBarrier(dstImage, RS_SHADER_RESOURCE, ImageSubResource { .baseMipLevel = mipLevel });
        }

        // all mip levels have been transitioned into this state
        rq << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
    }
};

#pragma endregion Render commands

#pragma region TextureMipmapRenderer

void TextureMipmapRenderer::RenderMipmaps(const Handle<Texture>& texture)
{
    Assert(texture.IsValid());

    GpuImageViewRef textureImageView = g_renderInterface->textureViewCache->GetOrCreate(texture);
    Assert(textureImageView.IsValid());

    const uint32 numMipLevels = texture->GetTextureDesc().NumMips();
    const Vec3u extent = texture->GetExtent();

    Array<GpuImageViewRef> mipImageViews;
    mipImageViews.Resize(numMipLevels);

    Array<FullScreenPass*> passes;
    passes.Resize(numMipLevels);

    for (uint32 mipLevel = 0; mipLevel < numMipLevels; mipLevel++)
    {
        const uint32 mipWidth = MathUtil::Max(1u, extent.x >> mipLevel);
        const uint32 mipHeight = MathUtil::Max(1u, extent.y >> mipLevel);

        GpuImageViewRef mipImageView = g_renderBackend->MakeImageView(texture->GetGpuImage(), mipLevel, 1, 0, texture->NumArrayLayers());
        DeferCreate(mipImageView);

        mipImageViews[mipLevel] = std::move(mipImageView);

        FullScreenPass* pass = new FullScreenPass(
            ShaderDefinition(NAME("GenerateMipmaps")),
            texture->GetFormat(),
            Vec2u { mipWidth, mipHeight },
            nullptr);

        pass->Create();

        passes[mipLevel] = pass;
    }

    RenderTextureMipmapLevelsTask task(
        texture->GetGpuImage(),
        textureImageView,
        std::move(mipImageViews),
        std::move(passes));

    if (IsOnThread(g_renderThread))
    {
        task();
    }
    else
    {
        g_renderThreadInstance->GetScheduler().Enqueue(std::move(task));
    }
}

#pragma endregion TextureMipmapRenderer

} // namespace Hyperion
