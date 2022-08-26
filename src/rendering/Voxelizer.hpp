#ifndef HYPERION_V2_VOXELIZER_H
#define HYPERION_V2_VOXELIZER_H

#include "Base.hpp"
#include "../scene/Scene.hpp"
#include "RenderPass.hpp"
#include "Framebuffer.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"
#include "Atomics.hpp"
#include <Types.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion::v2 {

class Engine;

class Voxelizer : public EngineComponentBase<STUB_CLASS(Voxelizer)>
{
    struct Fragment
    {
        UInt32 x, y;
    };

public:
    static constexpr UInt octree_depth = 10;
    static constexpr UInt voxel_map_size = 1 << octree_depth;
    static constexpr UInt default_fragment_list_buffer_size = 20000 * sizeof(Fragment);

    Voxelizer();
    Voxelizer(const Voxelizer &other) = delete;
    Voxelizer &operator=(const Voxelizer &other) = delete;
    ~Voxelizer();

    Handle<Scene> &GetScene() { return m_scene; }
    const Handle<Scene> &GetScene() const { return m_scene; }

    UInt32 NumFragments() const { return m_num_fragments; }

    void Init(Engine *engine);
    void Render(Engine *engine);

private:
    void CreatePipeline(Engine *engine);
    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffer(Engine *engine);
    void CreateDescriptors(Engine *engine);

    void ResizeFragmentListBuffer(Engine *engine);

    void RenderFragmentList(Engine *engine, bool count_mode);

    Handle<Scene> m_scene;

    std::unique_ptr<AtomicCounter> m_counter;
    std::unique_ptr<StorageBuffer> m_fragment_list_buffer;

    Handle<Framebuffer> m_framebuffer;
    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    Handle<RendererInstance> m_renderer_instance;

    std::vector<std::unique_ptr<renderer::Attachment>> m_attachments;

    UInt32 m_num_fragments;
};

} // namespace hyperion::v2

#endif