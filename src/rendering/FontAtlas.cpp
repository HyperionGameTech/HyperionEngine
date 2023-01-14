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

    virtual Result operator()()
    {
        auto commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

        auto &image = glyph->GetImage();

        ImageRect src_rect = { 0, 0, image->GetExtent()[0], image->GetExtent()[1] };
        ImageRect dest_rect = { location.x, location.y, location.x + src_rect.x1, location.y + src_rect.y1 };

        commands.Push([&](CommandBuffer *command_buffer) {
            // put src image in state for copying from
            image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
            // put dst image in state for copying to
            atlas->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

            //m_buffer->CopyFrom(command_buffer, staging_buffer, sizeof(value));
            atlas->GetImage()->Blit(command_buffer, image, src_rect, dest_rect);


            HYPERION_RETURN_OK;
        });
        //HYPERION_RETURN_OK;
        return commands.Execute(Engine::Get()->GetGPUInstance()->GetDevice());
    }

    Handle<Texture> atlas;
    Handle<Texture> glyph;

    Extent2D location;
};

#pragma endregion

void FontAtlas::RenderCharacter(Extent2D location, Extent2D dimensions, font::Glyph &glyph)
{
    PUSH_RENDER_COMMAND(RenderFontAtlas, m_atlas, glyph.GetTexture(), location);
}

renderer::Result FontAtlas::WriteTexture()
{
    const SizeType buffer_size = m_atlas_dimensions.x * m_atlas_dimensions.y;
    DebugLog(LogType::RenDebug, "buffer_size: %llu\n\ncell %u %u\n", buffer_size, m_cell_dimensions.x, m_cell_dimensions.y);
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

    ByteBuffer bitmap_data(buffer_size * 3);
    for (SizeType i = 0; i < buffer_size; i++) {
        UByte pixel = image_data[i];
        bitmap_data.Data()[i * 3] = 255 - pixel;
        bitmap_data.Data()[i * 3 + 1] = 255 - pixel;
        bitmap_data.Data()[i * 3 + 2] = 255 - pixel;
    }

    Bitmap<3> bitmap(m_atlas_dimensions.width, m_atlas_dimensions.height);
    bitmap.SetPixelsFromMemory(3, bitmap_data.Data(), (buffer_size));
    bitmap.Write("font.bmp");

    return result;
}



}; // namespace hyperion::v2

