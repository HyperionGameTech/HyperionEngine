/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_IMMEDIATE_MODE_HPP
#define HYPERION_IMMEDIATE_MODE_HPP

#include <Constants.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <math/Transform.hpp>
#include <math/Color.hpp>
#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Buffers.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class DebugDrawer;

enum class DebugDrawShape : uint
{
    SPHERE,
    BOX,
    PLANE,

    MAX
};

enum class DebugDrawType : uint
{
    DEFAULT,

    AMBIENT_PROBE,
    REFLECTION_PROBE,

    MAX
};

struct DebugDrawCommand
{
    DebugDrawShape  shape;
    DebugDrawType   type;
    Matrix4         transform_matrix;
    Color           color;
    ID<EnvProbe>    env_probe_id = ID<EnvProbe>::invalid;
};

class DebugDrawCommandList
{
public:
    friend class DebugDrawer;

    DebugDrawCommandList(DebugDrawer *debug_drawer)
        : m_debug_drawer(debug_drawer)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList &other)             = delete;
    DebugDrawCommandList &operator=(const DebugDrawCommandList &other)  = delete;

    void Sphere(const Vec3f &position, float radius = 0.15f, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Box(const Vec3f &position, const Vec3f &size = Vec3f::One(), Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Plane(const Vec3f &position, const Vec2f &size = Vec2f::One(), Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));

    void Commit();

private:
    DebugDrawer             *m_debug_drawer;
    Array<DebugDrawCommand> m_draw_commands;
    Mutex                   m_draw_commands_mutex;
};

class DebugDrawer
{
public:
    DebugDrawer();
    ~DebugDrawer();

    void Create();

    void Render(Frame *frame);

    UniquePtr<DebugDrawCommandList> CreateCommandList();
    void CommitCommands(DebugDrawCommandList &command_list);
    
    void Sphere(const Vec3f &position, float radius = 1.0f, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void AmbientProbeSphere(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id);
    void ReflectionProbeSphere(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id);
    void Box(const Vec3f &position, const Vec3f &size = Vec3f::One(), Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));
    void Plane(const FixedArray<Vec3f, 4> &points, Color color = Color(0.0f, 1.0f, 0.0f, 1.0f));

private:
    void UpdateDrawCommands();

    ShaderRef                                           m_shader;
    Handle<RenderGroup>                                 m_render_group;

    FixedArray<Handle<Mesh>, uint(DebugDrawShape::MAX)> m_shapes;
    Array<DebugDrawCommand>                             m_draw_commands;
    Array<DebugDrawCommand>                             m_draw_commands_pending_addition;
    AtomicVar<uint32>                                   m_num_draw_commands_pending_addition { 0 };
    Mutex                                               m_draw_commands_mutex;

    FixedArray<GPUBufferRef, max_frames_in_flight>      m_instance_buffers;
};

} // namespace hyperion

#endif