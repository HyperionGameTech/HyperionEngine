#ifndef HYPERION_V2_VOXELIZER_H
#define HYPERION_V2_VOXELIZER_H

#include <core/Base.hpp>
#include <scene/Scene.hpp>
#include <core/Containers.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/Buffers.hpp>
#include <Types.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererFrame.hpp>

namespace hyperion::v2 {

using renderer::Frame;

class Engine;
class Scene;
class Camera;
class AtomicCounter;

class Voxelizer : public EngineComponentBase<STUB_CLASS(Voxelizer)>
{
    using Fragment = ShaderVec2<UInt32>;

public:
    static constexpr UInt octree_depth = 10;
    static constexpr UInt voxel_map_size = 1 << octree_depth;
    static constexpr UInt default_fragment_list_buffer_size = 20000 * sizeof(Fragment);

    Voxelizer();
    Voxelizer(const Voxelizer &other) = delete;
    Voxelizer &operator=(const Voxelizer &other) = delete;
    ~Voxelizer();

    StorageBuffer *GetFragmentListBuffer() const
        { return m_fragment_list_buffer.get(); }

    AtomicCounter *GetAtomicCounter() const
        { return m_counter.get(); }

    UInt32 NumFragments() const
        { return m_num_fragments; }

    void Init();
    void Update(GameCounter::TickUnit delta);
    void CollectEntities(Scene *scene);
    void Render(Frame *frame, Scene *scene);

private:
    void CreateBuffers();
    void CreateShader();
    void CreateFramebuffer();
    void CreateDescriptors();

    void ResizeFragmentListBuffer(Frame *frame);

    void RenderFragmentList(Frame *frame, Scene *scene, bool count_mode);

    Handle<Camera> m_camera;
    RenderList m_render_list;

    std::unique_ptr<AtomicCounter> m_counter;
    std::unique_ptr<StorageBuffer> m_fragment_list_buffer;

    Handle<Framebuffer> m_framebuffer;
    Handle<Shader> m_shader;

    std::vector<std::unique_ptr<renderer::Attachment>> m_attachments;

    UInt32 m_num_fragments;
};

} // namespace hyperion::v2

#endif