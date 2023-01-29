//
// Created by emd22 on 24/12/22.
//
#include <rendering/FontAtlas.hpp>
#include <rendering/RenderCommands.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ImageRect;

#pragma region Render commands

struct RENDER_COMMAND(RenderFontAtlas) : RenderCommand {
    RENDER_COMMAND (RenderFontAtlas)(const Handle<Texture> &atlas, const Handle<Texture> &glyph, const Extent2D location)
        : atlas(atlas),
          glyph(glyph),
          location(location)
    {}

    Handle<Texture> atlas;
    Handle<Texture> glyph;

    Extent2D location;
};

#pragma endregion

void FontAtlas::RenderCharacter(Extent2D location, Extent2D dimensions, font::Glyph &glyph)
{
    Threads::AssertOnThread(THREAD_RENDER);
    auto commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

    auto &image = glyph.GetTexture()->GetImage();

    HYP_SYNC_RENDER();

    ImageRect src_rect = { 10, 10, image->GetExtent().width+10, image->GetExtent().height+10 };
    ImageRect dest_rect = { location.x, location.y, location.x + image->GetExtent()[0], location.y + image->GetExtent()[1] };

    commands.Push([&](CommandBuffer *command_buffer) {
        // put src image in state for copying from
        image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        // put dst image in state for copying to
        m_atlas->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        //m_buffer->CopyFrom(command_buffer, staging_buffer, sizeof(value));
        m_atlas->GetImage()->Blit(command_buffer, image, src_rect, dest_rect);


        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(commands.Execute(Engine::Get()->GetGPUInstance()->GetDevice()));

    //PUSH_RENDER_COMMAND(RenderFontAtlas, m_atlas, glyph.GetTexture(), location);
}

renderer::Result FontAtlas::WriteToBuffer(RC<ByteBuffer> &buffer)
{
    const SizeType buffer_size = m_atlas_dimensions.x * m_atlas_dimensions.y;
    renderer::Result result = renderer::Result::OK;

    const auto gpu_device = Engine::Get()->GetGPUDevice();
    ByteBuffer image_data(buffer_size);

    StagingBuffer texture_staging_buffer;
    HYPERION_BUBBLE_ERRORS(texture_staging_buffer.Create(gpu_device, buffer_size));
    texture_staging_buffer.Memset(gpu_device, buffer_size, 0xaa);

    if (!result) {
        HYPERION_IGNORE_ERRORS(texture_staging_buffer.Destroy(gpu_device));

        return result;
    }

    auto commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

    AssertThrow(m_atlas);
    AssertThrow(m_atlas->GetImage());
    AssertThrow(m_atlas->GetImage()->GetGPUImage());



    commands.Push([&](CommandBuffer *cmd) {

        // put src image in state for copying from
        m_atlas->GetImage()->GetGPUImage()->InsertBarrier(cmd, renderer::ResourceState::COPY_SRC);
        // put dst image in state for copying to
        //m_atlas->GetImage().GetGPUImage()->InsertBarrier(cmd, renderer::ResourceState::COPY_DST);

        m_atlas->GetImage()->GetGPUImage()->Read(gpu_device, buffer_size, image_data.Data());

        //m_atlas->GetImage().CopyToBuffer(cmd, &texture_staging_buffer);

        HYPERION_RETURN_OK;
    });

    HYPERION_PASS_ERRORS(commands.Execute(gpu_device), result);
    //texture_staging_buffer.Read(Engine::Get()->GetGPUInstance()->GetDevice(), buffer_size, image_data.Data());
    HYPERION_PASS_ERRORS(texture_staging_buffer.Destroy(gpu_device), result);

    buffer.Set(image_data);

    HYPERION_RETURN_OK;
}



}; // namespace hyperion::v2

