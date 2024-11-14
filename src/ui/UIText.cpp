/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIText.hpp>
#include <ui/UIStage.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include <core/logging/Logger.hpp>

#include <core/system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

static float GetWindowDPIFactor()
{
    return g_engine->GetAppContext()->GetMainWindow()->IsHighDPI()
        ? 2.0f
        : 1.0f;
}

#pragma region UITextRenderer

struct alignas(16) UITextCharacterShaderData
{
    Matrix4 transform;
    Vec2f   texcoord_start;
    Vec2f   texcoord_end;
};

struct alignas(16) UITextUniforms
{
    Matrix4 projection_matrix;
    Vec4f   color;
    Vec2f   text_aabb_min;
    Vec2f   text_aabb_max;
};

class UITextRenderer
{
public:
    UITextRenderer(Vec2u size)
    {
        static constexpr SizeType initial_character_instance_buffer_size = sizeof(UITextCharacterShaderData) * 1024;

        { // instance buffer
            m_character_instance_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
            DeferCreate(m_character_instance_buffer, g_engine->GetGPUDevice(), initial_character_instance_buffer_size);
        }

        { // uniform buffer
            m_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);
            DeferCreate(m_uniform_buffer, g_engine->GetGPUDevice(), sizeof(UITextUniforms));
        }

        { // shader
            m_shader = g_shader_manager->GetOrCreate(NAME("UIText"), ShaderProperties(static_mesh_vertex_attributes));
            AssertThrow(m_shader.IsValid());
        }

        { // mesh
            // m_quad_mesh = UIObjectQuadMeshHelper::GetQuadMesh();
            
            m_quad_mesh = MeshBuilder::Quad();
            InitObject(m_quad_mesh);
        }

        { // framebuffer
            m_framebuffer = MakeRenderObject<Framebuffer>(
                Vec2u(Vec2f(size) * GetWindowDPIFactor()),
                renderer::RenderPassStage::PRESENT,
                renderer::RenderPassMode::RENDER_PASS_INLINE
            );
            
            m_framebuffer->AddAttachment(
                0,
                InternalFormat::RGBA8,
                ImageType::TEXTURE_TYPE_2D,
                renderer::RenderPassStage::SHADER,
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE
            );

            DeferCreate(m_framebuffer, g_engine->GetGPUDevice());
        }
    }

    UITextRenderer(const UITextRenderer &other)             = delete;
    UITextRenderer &operator=(const UITextRenderer &other)  = delete;

    UITextRenderer(UITextRenderer &&other) noexcept
        : m_framebuffer(std::move(other.m_framebuffer)),
          m_shader(std::move(other.m_shader)),
          m_quad_mesh(std::move(other.m_quad_mesh)),
          m_character_instance_buffer(std::move(other.m_character_instance_buffer)),
          m_uniform_buffer(std::move(other.m_uniform_buffer)),
          m_render_group(std::move(other.m_render_group))
    {
    }

    UITextRenderer &operator=(UITextRenderer &&other) noexcept
    {
        if (this != &other) {
            SafeRelease(std::move(m_shader));
            SafeRelease(std::move(m_framebuffer));
            SafeRelease(std::move(m_character_instance_buffer));
            SafeRelease(std::move(m_uniform_buffer));

            m_shader = std::move(other.m_shader);
            m_framebuffer = std::move(other.m_framebuffer);
            m_quad_mesh = std::move(other.m_quad_mesh);
            m_character_instance_buffer = std::move(other.m_character_instance_buffer);
            m_uniform_buffer = std::move(other.m_uniform_buffer);
            m_render_group = std::move(other.m_render_group);
        }

        return *this;
    }

    ~UITextRenderer()
    {
        SafeRelease(std::move(m_shader));
        SafeRelease(std::move(m_framebuffer));
        SafeRelease(std::move(m_character_instance_buffer));
        SafeRelease(std::move(m_uniform_buffer));
    }

    HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    void RenderText(Frame *frame, const UITextRenderData &render_data)
    {
        HYP_SCOPE;
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        const uint frame_index = frame->GetFrameIndex();

        m_framebuffer->BeginCapture(frame->GetCommandBuffer(), frame_index);
        
        const SizeType characters_byte_size = render_data.characters.Size() * sizeof(UITextCharacterShaderData);

        bool was_character_instance_buffer_rebuilt = false;

        if (characters_byte_size > m_character_instance_buffer->Size()) {
            ByteBuffer previous_data;
            previous_data.SetSize(m_character_instance_buffer->Size());

            m_character_instance_buffer->Read(
                g_engine->GetGPUDevice(),
                m_character_instance_buffer->Size(),
                previous_data.Data()
            );

            HYPERION_ASSERT_RESULT(m_character_instance_buffer->EnsureCapacity(
                g_engine->GetGPUDevice(),
                characters_byte_size,
                &was_character_instance_buffer_rebuilt
            ));

            m_character_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                previous_data.Size(),
                previous_data.Data()
            );
        }

        { // copy characters shader data
            Array<UITextCharacterShaderData> characters;
            characters.Resize(render_data.characters.Size());

            for (SizeType index = 0; index < render_data.characters.Size(); index++) {
                characters[index] = UITextCharacterShaderData {
                    render_data.characters[index].transform,
                    render_data.characters[index].texcoord_start,
                    render_data.characters[index].texcoord_end
                };
            }

            m_character_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                0,
                characters_byte_size,
                characters.Data()
            );
        }

        { // copy uniform
            // // testing
            // g_engine->GetRenderData()->cameras.UpdateBuffer(g_engine->GetGPUDevice(), frame_index);
            
            Matrix4 projection_matrix = Matrix4::Orthographic(
                0.0f, 1.0f,
                0.0f, 1.0f,
                -1.0f, 1.0f
            );

            UITextUniforms uniforms;
            uniforms.projection_matrix = projection_matrix;
            uniforms.color = render_data.color;
            uniforms.text_aabb_min = Vec2f(render_data.aabb.min.x, render_data.aabb.min.y);
            uniforms.text_aabb_max = Vec2f(render_data.aabb.max.x, render_data.aabb.max.y);

            m_uniform_buffer->Copy(
                g_engine->GetGPUDevice(),
                0,
                sizeof(UITextUniforms),
                &uniforms
            );
        }

        CreateRenderGroup(render_data.font_atlas_texture);

        const uint descriptor_set_index = m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("UITextDescriptorSet"));
        AssertThrow(descriptor_set_index != ~0u);

        if (was_character_instance_buffer_rebuilt) {
            const DescriptorSetRef &descriptor_set = m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(descriptor_set_index, frame_index);
            descriptor_set->SetElement(NAME("CharacterInstanceBuffer"), m_character_instance_buffer);
            descriptor_set->Update(g_engine->GetGPUDevice());
        }

        m_render_group->GetPipeline()->Bind(frame->GetCommandBuffer());

        if (renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial()) {
            m_render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                frame->GetCommandBuffer(),
                frame_index,
                m_render_group->GetPipeline(),
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    },
                    {
                        NAME("Object"),
                        {
                            { NAME("MaterialsBuffer"), 0 },
                            { NAME("SkeletonsBuffer"), 0 },
                            { NAME("EntityInstanceBatchesBuffer"), 0 }
                        }
                    }
                }
            );
        } else {
            m_render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                frame->GetCommandBuffer(),
                frame_index,
                m_render_group->GetPipeline(),
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    },
                    {
                        NAME("Object"),
                        {
                            { NAME("SkeletonsBuffer"), 0 },
                            { NAME("EntityInstanceBatchesBuffer"), 0 }
                        }
                    }
                }
            );
        }

        m_quad_mesh->Render(frame->GetCommandBuffer(), render_data.characters.Size());

        m_framebuffer->EndCapture(frame->GetCommandBuffer(), frame_index);

        g_engine->GetRenderState().UnbindCamera();
    }

private:
    DescriptorTableRef CreateDescriptorTable(const Handle<Texture> &font_atlas_texture)
    {
        renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
        AssertThrow(descriptor_table != nullptr);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("UITextDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("CharacterInstanceBuffer"), m_character_instance_buffer);
            descriptor_set->SetElement(NAME("UITextUniforms"), m_uniform_buffer);
            descriptor_set->SetElement(NAME("FontAtlasTexture"), font_atlas_texture->GetImageView());
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        return descriptor_table;
    }

    void CreateRenderGroup(const Handle<Texture> &font_atlas_texture)
    {
        Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
            m_shader,
            RenderableAttributeSet(
                MeshAttributes {
                    .vertex_attributes = static_mesh_vertex_attributes
                },
                MaterialAttributes {
                    .bucket         = Bucket::BUCKET_TRANSLUCENT,
                    .fill_mode      = FillMode::FILL,
                    .blend_function = BlendFunction::None(),
                    .cull_faces     = FaceCullMode::NONE
                }
            ),
            CreateDescriptorTable(font_atlas_texture),
            RenderGroupFlags::NONE
        );

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        m_render_group = std::move(render_group);
    }

    FramebufferRef          m_framebuffer;
    ShaderRef               m_shader;
    Handle<Mesh>            m_quad_mesh;
    GPUBufferRef            m_character_instance_buffer;
    GPUBufferRef            m_uniform_buffer;
    Handle<RenderGroup>     m_render_group;
};

#pragma endregion UITextRenderer

struct FontAtlasCharacterIterator
{
    Vec2f           placement;
    Vec2f           atlas_pixel_size;
    Vec2f           cell_dimensions;

    utf::u32char    char_value;

    Vec2i           char_offset;

    Vec2f           glyph_dimensions;
    Vec2f           glyph_scaling;

    float           bearing_y;
    float           char_width;
};

template <class Callback>
static void ForEachCharacter(const FontAtlas &font_atlas, const String &text, const Vec2i &parent_bounds, float text_size, Callback &&callback)
{
    HYP_SCOPE;

    Vec2f placement;

    const SizeType length = text.Length();
    
    const Vec2f cell_dimensions = Vec2f(font_atlas.GetCellDimensions()) / 64.0f;
    AssertThrowMsg(cell_dimensions.x * cell_dimensions.y != 0.0f, "Cell dimensions are invalid");

    const float cell_dimensions_ratio = cell_dimensions.x / cell_dimensions.y;

    const Handle<Texture> &main_texture_atlas = font_atlas.GetAtlasTextures().GetMainAtlas();
    AssertThrowMsg(main_texture_atlas.IsValid(), "Main texture atlas is invalid");

    const Vec2f atlas_pixel_size = Vec2f::One() / Vec2f(main_texture_atlas->GetExtent().GetXY());
    
    struct
    {
        Array<FontAtlasCharacterIterator>   chars;
    } current_word;

    const auto IterateCurrentWord = [&current_word, &callback]()
    {
        for (const FontAtlasCharacterIterator &character_iterator : current_word.chars) {
            callback(character_iterator);
        }

        current_word = { };
    };

    for (SizeType i = 0; i < length; i++) {
        UITextCharacter character { };

        const utf::u32char ch = text.GetChar(i);

        if (ch == utf::u32char(' ')) {
            if (current_word.chars.Any() && parent_bounds.x != 0 && (current_word.chars.Back().placement.x + current_word.chars.Back().char_width) * text_size >= float(parent_bounds.x)) {
                // newline
                placement.x = 0.0f;
                placement.y += cell_dimensions.y;

                const Vec2f placement_origin = placement;
                const Vec2f placement_offset = current_word.chars.Front().placement - placement_origin;

                for (FontAtlasCharacterIterator &character_iterator : current_word.chars) {
                    character_iterator.placement -= placement_offset;
                }
            } else {
                // add room for space
                placement.x += cell_dimensions.x * 0.5f;
            }

            IterateCurrentWord();

            continue;
        }

        if (ch == utf::u32char('\n')) {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += cell_dimensions.y;

            IterateCurrentWord();

            continue;
        }

        Optional<Glyph::Metrics> glyph_metrics = font_atlas.GetGlyphMetrics(ch);

        if (!glyph_metrics.HasValue()) {
            // @TODO Add a placeholder character for missing glyphs
            continue;
        }

        if (glyph_metrics->metrics.width == 0 || glyph_metrics->metrics.height == 0) {
            // empty width or height will cause issues
            continue;
        }

        FontAtlasCharacterIterator &character_iterator = current_word.chars.EmplaceBack();
        character_iterator.char_value = ch;
        character_iterator.placement = placement;
        character_iterator.atlas_pixel_size = atlas_pixel_size;
        character_iterator.cell_dimensions = cell_dimensions;
        character_iterator.char_offset = glyph_metrics->image_position;
        character_iterator.glyph_dimensions = Vec2f { float(glyph_metrics->metrics.width), float(glyph_metrics->metrics.height) } / 64.0f;
        character_iterator.glyph_scaling = Vec2f(character_iterator.glyph_dimensions) / (Vec2f(cell_dimensions));
        character_iterator.bearing_y = float(glyph_metrics->metrics.height - glyph_metrics->metrics.bearing_y) / 64.0f;
        character_iterator.char_width = float(glyph_metrics->metrics.advance / 64) / 64.0f;

        placement.x += character_iterator.char_width;
    }

    IterateCurrentWord();
}

static BoundingBox CalculateTextAABB(const FontAtlas &font_atlas, const String &text, const Vec2i &parent_bounds, float text_size, bool include_bearing)
{
    HYP_SCOPE;

    BoundingBox aabb;

    ForEachCharacter(font_atlas, text, parent_bounds, text_size, [include_bearing, &aabb](const FontAtlasCharacterIterator &iter)
    {
        BoundingBox character_aabb;

        if (include_bearing) {
            const float offset_y = (iter.cell_dimensions.y - iter.glyph_dimensions.y) + iter.bearing_y;

            character_aabb = character_aabb
                .Union(Vec3f(iter.placement.x, iter.placement.y + offset_y, 0.0f))
                .Union(Vec3f(iter.placement.x + iter.glyph_dimensions.x, iter.placement.y + offset_y + iter.cell_dimensions.y, 0.0f));
        } else {
            character_aabb = character_aabb
                .Union(Vec3f(iter.placement.x, iter.placement.y, 0.0f))
                .Union(Vec3f(iter.placement.x + iter.glyph_dimensions.x, iter.placement.y + iter.cell_dimensions.y, 0.0f));
        }

        aabb = aabb.Union(character_aabb);
    });

    return aabb;
}

#pragma region Render commands

struct RENDER_COMMAND(UpdateUITextRenderData) : renderer::RenderCommand
{
    String                  text;
    RC<UITextRenderData>    render_data;
    Vec4f                   color;
    Vec2i                   size;
    Vec2i                   parent_bounds;
    float                   text_size;
    BoundingBox             aabb;
    RC<FontAtlas>           font_atlas;
    Handle<Texture>         font_atlas_texture;

    RENDER_COMMAND(UpdateUITextRenderData)(const String &text, const RC<UITextRenderData> &render_data, Vec4f color, Vec2i size, Vec2i parent_bounds, float text_size, const BoundingBox &aabb, const RC<FontAtlas> &font_atlas, const Handle<Texture> &font_atlas_texture)
        : text(text),
          render_data(render_data),
          color(color),
          size(size),
          parent_bounds(parent_bounds),
          text_size(text_size),
          aabb(aabb),
          font_atlas(font_atlas),
          font_atlas_texture(font_atlas_texture)
    {
    }

    virtual ~RENDER_COMMAND(UpdateUITextRenderData)() override = default;

    virtual renderer::Result operator()() override
    {
        if (!render_data) {
            return { renderer::Result::RENDERER_ERR, "Cannot update UI text render data, invalid render data pointer set" };
        }

        if (!font_atlas) {
            return { renderer::Result::RENDERER_ERR, "Cannot update UI text render data, invalid font atlas texture set" };
        }

        render_data->characters.Clear();

        ForEachCharacter(*font_atlas, text, parent_bounds, text_size, [&](const FontAtlasCharacterIterator &iter)
        {
            Transform character_transform;
            character_transform.SetScale(Vec3f(iter.glyph_dimensions.x, iter.glyph_dimensions.y, 1.0f));
            character_transform.GetTranslation().y += iter.cell_dimensions.y - iter.glyph_dimensions.y;
            character_transform.GetTranslation().y += iter.bearing_y;
            character_transform.GetTranslation() += Vec3f(iter.placement.x, iter.placement.y, 0.0f);
            character_transform.UpdateMatrix();

            UITextCharacter character { };
            character.transform = character_transform.GetMatrix();
            character.texcoord_start = Vec2f(iter.char_offset) * iter.atlas_pixel_size;
            character.texcoord_end = (Vec2f(iter.char_offset) + (iter.glyph_dimensions * 64.0f)) * iter.atlas_pixel_size;

            render_data->characters.PushBack(character);
        });

        render_data->color = color;
        render_data->size = size;
        render_data->aabb = aabb;
        render_data->font_atlas = std::move(font_atlas);
        render_data->font_atlas_texture = std::move(font_atlas_texture);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RepaintUIText) : renderer::RenderCommand
{
    RC<UITextRenderData>    render_data;
    Handle<Texture>         texture;
    UITextRenderer          text_renderer;

    RENDER_COMMAND(RepaintUIText)(const RC<UITextRenderData> &render_data, const Handle<Texture> &texture, UITextRenderer &&text_renderer)
        : render_data(render_data),
          texture(texture),
          text_renderer(std::move(text_renderer))
    {
    }

    virtual ~RENDER_COMMAND(RepaintUIText)() override = default;

    virtual renderer::Result operator()() override
    {
        if (!render_data) {
            return { renderer::Result::RENDERER_ERR, "Cannot repaint UI text, invalid render data pointer set" };
        }

        if (!texture.IsValid()) {
            return { renderer::Result::RENDERER_ERR, "Cannot repaint UI text, invalid texture set" };
        }

        if (render_data->size.x * render_data->size.y <= 0) {
            HYP_LOG_ONCE(UI, LogLevel::WARNING, "Cannot repaint UI text, size is zero");

            HYPERION_RETURN_OK;
        }

        Frame *frame = g_engine->GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();

        text_renderer.RenderText(frame, *render_data);

        const ImageRef &src_image = text_renderer.GetFramebuffer()->GetAttachment(0)->GetImage();
        const ImageRef &dst_image = texture->GetImage();

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        dst_image->Blit(frame->GetCommandBuffer(), src_image);

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

        HYPERION_RETURN_OK;
    };
};

#pragma endregion Render commands

#pragma region UIText

UIText::UIText()
    : UIObject(UIObjectType::TEXT),
      m_render_data(MakeRefCountedPtr<UITextRenderData>())
{
    m_text_color = Color(Vec4f::One());

    // // temp testing
    // OnMouseHover.Bind([this](...)
    // {
    //     // m_texture.Reset();

    //     // SetNeedsRepaintFlag(true);
    //     SetIsVisible(false);
    //     SetIsVisible(true);
    //     return UIEventHandlerResult::OK;
    // }).Detach();
}

const RC<UITextRenderData> &UIText::GetRenderData() const
{
    return m_render_data;
}

void UIText::Init()
{
    HYP_SCOPE;

    UIObject::Init();
}

void UIText::SetText(const String &text)
{
    HYP_SCOPE;

    UIObject::SetText(text);

    if (!IsInit()) {
        return;
    }

    SetNeedsRepaintFlag(true);

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

const RC<FontAtlas> &UIText::GetFontAtlasOrDefault() const
{
    HYP_SCOPE;

    const UIStage *stage = GetStage();

    if (!stage) {
        return m_font_atlas;
    }

    return m_font_atlas != nullptr
        ? m_font_atlas
        : stage->GetDefaultFontAtlas();
}

void UIText::SetFontAtlas(const RC<FontAtlas> &font_atlas)
{
    HYP_SCOPE;

    m_font_atlas = font_atlas;

    if (!IsInit()) {
        return;
    }

    SetNeedsRepaintFlag(true);

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

void UIText::UpdateTextAABB()
{
    HYP_SCOPE;

    if (const RC<FontAtlas> &font_atlas = GetFontAtlasOrDefault()) {
        const Vec2i parent_bounds = GetParentBounds();
        const float text_size = GetTextSize();

        m_text_aabb_with_bearing = CalculateTextAABB(*font_atlas, m_text, parent_bounds, text_size, true);
        m_text_aabb_without_bearing = CalculateTextAABB(*font_atlas, m_text, parent_bounds, text_size, false);
    } else {
        HYP_LOG_ONCE(UI, LogLevel::WARNING, "No font atlas for UIText {}", GetName());
    }
}

void UIText::UpdateRenderData()
{
    HYP_SCOPE;

    if (m_locked_updates & UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA | UIObjectUpdateType::UPDATE_CHILDREN_TEXT_RENDER_DATA);

    // Only update render data if computed visibility is true (visibile)
    // When this changes to be true, UpdateRenderData will be called - no need to update it if we are not visible
    if (!GetComputedVisibility()) {
        return;
    }

    HYP_LOG(UI, LogLevel::DEBUG, "Update render data for text '{}'", GetText());

    if (const RC<FontAtlas> &font_atlas = GetFontAtlasOrDefault()) {
        const float text_size = GetTextSize();

        m_current_font_atlas_texture = font_atlas->GetAtlasTextures().GetAtlasForPixelSize(text_size);

        if (!m_current_font_atlas_texture.IsValid()) {
            HYP_LOG_ONCE(UI, LogLevel::WARNING, "No font atlas texture for text size {}", text_size);
        }

        UpdateMaterial(false);
        UpdateMeshData(false);

        SetNeedsRepaintFlag(true);
    }

    const NodeProxy &node = GetNode();

    const Vec2i size = GetActualSize();

    // if (size.x > 0 && size.y > 0) {
    //     const Vec2u size_clamped = Vec2u(MathUtil::Max(size, Vec2i::One()));

    //     if (!m_texture.IsValid() || m_texture->GetExtent().GetXY() != size_clamped) {
    //         m_texture = CreateObject<Texture>(TextureDesc {
    //             ImageType::TEXTURE_TYPE_2D,
    //             InternalFormat::RGBA8,
    //             Vec3u { uint32(size_clamped.x * GetWindowDPIFactor()), uint32(size_clamped.y * GetWindowDPIFactor()), 1 },
    //             FilterMode::TEXTURE_FILTER_NEAREST,
    //             FilterMode::TEXTURE_FILTER_NEAREST,
    //             WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    //         });
            
    //         InitObject(m_texture);

    //         UpdateMaterial(false);
    //         UpdateMeshData(false);
    //     }

    //     SetNeedsRepaintFlag(true);
    // } else {
    //     m_texture.Reset();

    //     SetNeedsRepaintFlag(false);
    // }
}

void UIText::UpdateMeshData_Internal()
{
    HYP_SCOPE;

    UIObject::UpdateMeshData_Internal();

    const RC<FontAtlas> &font_atlas = GetFontAtlasOrDefault();

    if (!font_atlas) {
        HYP_LOG_ONCE(UI, LogLevel::WARNING, "No font atlas, cannot update text mesh data");

        return;
    }

    MeshComponent &mesh_component = GetScene()->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());
    mesh_component.instance_data.transforms.Clear();

    const Vec2i size = GetActualSize();
    const Vec2f position = GetAbsolutePosition();

    const float text_size = GetTextSize();

    ForEachCharacter(*font_atlas, m_text, GetParentBounds(), text_size, [&](const FontAtlasCharacterIterator &iter)
    {
        BoundingBox character_aabb;

        const float offset_y = (iter.cell_dimensions.y - iter.glyph_dimensions.y) + iter.bearing_y;

        character_aabb = character_aabb
            .Union(Vec3f(iter.placement.x, iter.placement.y + offset_y, 0.0f))
            .Union(Vec3f(iter.placement.x + iter.glyph_dimensions.x, iter.placement.y + offset_y + iter.cell_dimensions.y, 0.0f));

        BoundingBox character_aabb_clamped = {
            Vec3f { float(position.x), float(position.y), 0.0f } + (Vec3f { iter.placement.x, iter.placement.y + offset_y, 0.0f } * text_size),
            Vec3f { float(position.x), float(position.y), 0.0f } + (Vec3f { iter.placement.x + iter.glyph_dimensions.x, iter.placement.y + offset_y + iter.cell_dimensions.y, 0.0f } * text_size)
        };

        character_aabb_clamped = character_aabb_clamped.Intersection(m_aabb_clamped);

            character.texcoord_start = Vec2f(iter.char_offset) * iter.atlas_pixel_size;
            character.texcoord_end = (Vec2f(iter.char_offset) + (iter.glyph_dimensions * 64.0f)) * iter.atlas_pixel_size;

        Matrix4 instance_transform;
        instance_transform[0][0] = character_aabb_clamped.max.x - character_aabb_clamped.min.x;
        instance_transform[1][1] = character_aabb_clamped.max.y - character_aabb_clamped.min.y;
        instance_transform[2][2] = 1.0f;

        instance_transform[0][3] = character_aabb_clamped.min.x;
        instance_transform[1][3] = character_aabb_clamped.min.y;
        instance_transform[3][3] = 1.0f;

        mesh_component.instance_data.transforms.PushBack(instance_transform);
    });

    HYP_LOG(UI, LogLevel::DEBUG, "Text \"{}\" has {} characters", m_text, mesh_component.instance_data.transforms.Size());

    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

bool UIText::Repaint_Internal()
{
    if (!m_texture.IsValid()) {
        HYP_LOG_ONCE(UI, LogLevel::WARNING, "Cannot repaint UI text for element: {}, no texture set", GetName());

        return false;
    }

    Vec2i size = GetActualSize();

    if (size.x * size.y <= 0) {
        HYP_LOG_ONCE(UI, LogLevel::WARNING, "Cannot repaint UI text for element: {}, size <= zero", GetName());

        return false;
    }

    size = MathUtil::Max(size, Vec2i::One());

    HYP_LOG(UI, LogLevel::DEBUG, "Repaint text '{}', {}, {}", GetText(), m_text_aabb_without_bearing, m_texture->GetExtent());

    

    // if (font_atlas != nullptr && font_atlas_texture != nullptr) {
    //     PUSH_RENDER_COMMAND(UpdateUITextRenderData, m_text, m_render_data, Vec4f(GetTextColor()), size, GetParentBounds(), GetTextSize(), m_text_aabb_with_bearing, font_atlas, font_atlas_texture);
    // }
    
    // PUSH_RENDER_COMMAND(RepaintUIText, m_render_data, m_texture, UITextRenderer(m_texture->GetExtent().GetXY()));

    return true;
}

MaterialAttributes UIText::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

Material::ParameterTable UIText::GetMaterialParameters() const
{
    return {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(GetTextColor()) }
    };
}

Material::TextureSet UIText::GetMaterialTextures() const
{
    if (!m_current_font_atlas_texture.IsValid()) {
        return UIObject::GetMaterialTextures();
    }

    return {
        { MaterialTextureKey::ALBEDO_MAP, m_current_font_atlas_texture }
    };
}

void UIText::Update_Internal(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    if (m_deferred_updates) {
        if (m_deferred_updates & UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA) {
            UpdateRenderData();
        }
    }

    UIObject::Update_Internal(delta);
}

void UIText::UpdateSize_Internal(bool update_children)
{
    HYP_SCOPE;

    UpdateTextAABB();

    UIObject::UpdateSize_Internal(update_children);

    const Vec3f extent_with_bearing = m_text_aabb_with_bearing.GetExtent();
    const Vec3f extent_without_bearing = m_text_aabb_without_bearing.GetExtent();

    m_actual_size.y *= extent_with_bearing.y / extent_without_bearing.y;
}

BoundingBox UIText::CalculateInnerAABB_Internal() const
{
    return m_text_aabb_without_bearing * Vec3f(Vec2f(GetTextSize()), 1.0f);
}

void UIText::OnComputedVisibilityChange_Internal()
{
    HYP_SCOPE;

    UIObject::OnComputedVisibilityChange_Internal();

    if (GetComputedVisibility()) {
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
    } else {
        HYP_LOG(UI, LogLevel::DEBUG, "UIText: Disposing texture for non-visible text \"{}\"", m_text);

        m_texture.Reset();
    }
}

void UIText::OnFontAtlasUpdate_Internal()
{
    HYP_SCOPE;

    UIObject::OnFontAtlasUpdate_Internal();

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

void UIText::OnTextSizeUpdate_Internal()
{
    HYP_SCOPE;

    UIObject::OnTextSizeUpdate_Internal();

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

Vec2i UIText::GetParentBounds() const
{
    Vec2i parent_bounds;

    if (const UIObject *parent = GetParentUIObject()) {
        const UIObjectSize parent_size = parent->GetSize();

        if (!(parent_size.GetFlagsX() & UIObjectSize::AUTO)) {
            parent_bounds.x = parent->GetActualSize().x;
        }

        if (!(parent_size.GetFlagsY() & UIObjectSize::AUTO)) {
            parent_bounds.y = parent->GetActualSize().y;
        }
    }

    return parent_bounds;
}

#pragma endregion UIText

} // namespace hyperion