#ifndef HYPERION_V2_IMMEDIATE_MODE_HPP
#define HYPERION_V2_IMMEDIATE_MODE_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <math/Transform.hpp>
#include <math/Color.hpp>
#include <math/Vector2.hpp>
#include <math/Vector3.hpp>

#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Renderer.hpp>

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
class ImmediateMode;

using renderer::Frame;
using renderer::UniformBuffer;
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
    Transform transform;
    Color color;
};

class DebugDrawCommandList
{
    friend class ImmediateMode;

    DebugDrawCommandList(ImmediateMode *immediate_mode)
        : m_immediate_mode(immediate_mode)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList &other) = delete;
    DebugDrawCommandList &operator=(const DebugDrawCommandList &other) = delete;

public:
    void Sphere(const Vector3 &position, Float radius = 1.0f, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Box(const Vector3 &position, const Vector3 &size = Vector3::one, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Plane(const Vector3 &position, const Vector2 &size = Vector2::one, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));

    void Commit();

private:
    ImmediateMode *m_immediate_mode;
    Array<DebugDrawCommand> m_draw_commands;
};

class ImmediateMode
{
public:
    ImmediateMode();
    ~ImmediateMode();

    void Create();
    void Destroy();

    void Render(
        
        Frame *frame
    );

    DebugDrawCommandList DebugDrawer()
        { return DebugDrawCommandList(this); }

    void CommitCommands(DebugDrawCommandList &&command_list);
    
    void Sphere(const Vector3 &position, Float radius = 1.0f, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Box(const Vector3 &position, const Vector3 &size = Vector3::one, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Plane(const Vector3 &position, const Vector2 &size = Vector2::one, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));

private:
    void UpdateDrawCommands();

    Handle<Shader> m_shader;
    Handle<RendererInstance> m_renderer_instance;

    FixedArray<Handle<Mesh>, UInt(DebugDrawShape::MAX)> m_shapes;
    Array<DebugDrawCommand> m_draw_commands;
    Array<DebugDrawCommand> m_draw_commands_pending_addition;

    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;

    std::atomic<Int64> m_num_draw_commands_pending_additon { 0 };
    std::mutex m_draw_commands_mutex;
};

} // namespace hyperion::v2

#endif