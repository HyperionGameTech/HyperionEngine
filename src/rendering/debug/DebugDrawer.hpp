#ifndef HYPERION_V2_IMMEDIATE_MODE_HPP
#define HYPERION_V2_IMMEDIATE_MODE_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <math/Transform.hpp>
#include <math/Color.hpp>
#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderGroup.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <Types.hpp>

#include <mutex>
#include <atomic>

#include <memory>

namespace hyperion::v2 {

class Engine;
class DebugDrawer;

using renderer::Frame;
using renderer::DescriptorSet;

enum class DebugDrawShape
{
    SPHERE,
    BOX,
    PLANE,

    MAX
};

struct DebugDrawCommand
{
    DebugDrawShape shape;
    Matrix4 transform_matrix;
    Color color;
};

class DebugDrawCommandList
{
    friend class DebugDrawer;

    DebugDrawCommandList(DebugDrawer *immediate_mode)
        : m_debug_drawer(immediate_mode)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList &other) = delete;
    DebugDrawCommandList &operator=(const DebugDrawCommandList &other) = delete;

public:
    void Sphere(const Vector3 &position, Float radius = 0.15f, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Box(const Vector3 &position, const Vector3 &size = Vector3::one, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Plane(const Vector3 &position, const Vector2 &size = Vector2::one, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));

    void Commit();

private:
    DebugDrawer *m_debug_drawer;
    Array<DebugDrawCommand> m_draw_commands;
};

class DebugDrawer
{
public:
    DebugDrawer();
    ~DebugDrawer();

    void Create();
    void Destroy();
    void Render(Frame *frame);

    void CommitCommands(DebugDrawCommandList &&command_list);
    
    void Sphere(const Vector3 &position, Float radius = 1.0f, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Box(const Vector3 &position, const Vector3 &size = Vector3::one, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Plane(const FixedArray<Vector3, 4> &points, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));

private:
    void UpdateDrawCommands();

    Handle<Shader> m_shader;
    Handle<RenderGroup> m_renderer_instance;

    FixedArray<Handle<Mesh>, UInt(DebugDrawShape::MAX)> m_shapes;
    Array<DebugDrawCommand> m_draw_commands;
    Array<DebugDrawCommand> m_draw_commands_pending_addition;

    FixedArray<DescriptorSetRef, max_frames_in_flight> m_descriptor_sets;

    std::atomic<Int64> m_num_draw_commands_pending_addition { 0 };
    std::mutex m_draw_commands_mutex;
};

} // namespace hyperion::v2

#endif