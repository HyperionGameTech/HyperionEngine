/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_IMMEDIATE_MODE_HPP
#define HYPERION_IMMEDIATE_MODE_HPP

#include <Constants.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/Handle.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Color.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <rendering/Buffers.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class RenderGroup;
class Mesh;
class DebugDrawer;
class DebugDrawCommandList;

enum class DebugDrawType : uint32
{
    DEFAULT,

    AMBIENT_PROBE,
    REFLECTION_PROBE,

    MAX
};

struct DebugDrawCommand
{
    Handle<Mesh>    mesh;
    DebugDrawType   type;
    Matrix4         transform_matrix;
    Color           color;
    ID<EnvProbe>    env_probe_id = ID<EnvProbe>::invalid;
};

class IDebugDrawShape
{
public:
    virtual ~IDebugDrawShape() = default;

    virtual const Handle<Mesh> &GetMesh() const = 0;
};

class HYP_API DebugDrawShapeBase : public IDebugDrawShape
{
public:
    DebugDrawShapeBase(DebugDrawCommandList &command_list, const Handle<Mesh> &mesh);

    virtual ~DebugDrawShapeBase() override = default;

    virtual const Handle<Mesh> &GetMesh() const override final
    {
        return m_mesh;
    }

protected:
    DebugDrawCommandList    &m_command_list;
    Handle<Mesh>            m_mesh;
};

class HYP_API SphereDebugDrawShape : public DebugDrawShapeBase
{
public:
    SphereDebugDrawShape(DebugDrawCommandList &command_list);

    virtual ~SphereDebugDrawShape() override = default;

    virtual void operator()(const Vec3f &position, float radius, const Color &color);
};

class HYP_API AmbientProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    AmbientProbeDebugDrawShape(DebugDrawCommandList &command_list)
        : SphereDebugDrawShape(command_list)
    {
    }

    virtual ~AmbientProbeDebugDrawShape() override = default;

    void operator()(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id);
};

class HYP_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(DebugDrawCommandList &command_list)
        : SphereDebugDrawShape(command_list)
    {
    }

    virtual ~ReflectionProbeDebugDrawShape() override = default;

    void operator()(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id);
};

class HYP_API BoxDebugDrawShape : public DebugDrawShapeBase
{
public:
    BoxDebugDrawShape(DebugDrawCommandList &command_list);

    virtual ~BoxDebugDrawShape() override = default;

    void operator()(const Vec3f &position, const Vec3f &size, const Color &color);
};

class HYP_API PlaneDebugDrawShape : public DebugDrawShapeBase
{
public:
    PlaneDebugDrawShape(DebugDrawCommandList &command_list);

    virtual ~PlaneDebugDrawShape() override = default;

    void operator()(const FixedArray<Vec3f, 4> &points, const Color &color);
};

class HYP_API OnScreenMessageDebugDrawShape : public IDebugDrawShape
{
public:
    OnScreenMessageDebugDrawShape(DebugDrawCommandList &command_list, UTF8StringView text, Color color);

    virtual ~OnScreenMessageDebugDrawShape() override = default;

    virtual const Handle<Mesh> &GetMesh() const override final
    {
        return Handle<Mesh>::empty;
    }

    // @TODO Implement using same technique as UIText
};

class DebugDrawCommandList
{
public:
    friend class DebugDrawer;

    DebugDrawCommandList(DebugDrawer &debug_drawer)
        : Sphere(*this),
          AmbientProbe(*this),
          ReflectionProbe(*this),
          Box(*this),
          Plane(*this),
          m_debug_drawer(debug_drawer)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList &other)             = delete;
    DebugDrawCommandList &operator=(const DebugDrawCommandList &other)  = delete;

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_num_draw_commands.Get(MemoryOrder::ACQUIRE) == 0;
    }

    void Push(DebugDrawCommand &&command);
    void Commit();

    SphereDebugDrawShape            Sphere;
    AmbientProbeDebugDrawShape      AmbientProbe;
    ReflectionProbeDebugDrawShape   ReflectionProbe;
    BoxDebugDrawShape               Box;
    PlaneDebugDrawShape             Plane;

private:
    DebugDrawer                     &m_debug_drawer;
    Array<DebugDrawCommand>         m_draw_commands;
    Mutex                           m_draw_commands_mutex;
    AtomicVar<uint32>               m_num_draw_commands { 0 };
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

private:
    void UpdateDrawCommands();

    DebugDrawCommandList                            m_default_command_list;

public: // Shapes
    SphereDebugDrawShape                            &Sphere;
    AmbientProbeDebugDrawShape                      &AmbientProbe;
    ReflectionProbeDebugDrawShape                   &ReflectionProbe;
    BoxDebugDrawShape                               &Box;
    PlaneDebugDrawShape                             &Plane;

private:
    ShaderRef                                       m_shader;
    Handle<RenderGroup>                             m_render_group;

    Array<DebugDrawCommand>                         m_draw_commands;
    Array<DebugDrawCommand>                         m_draw_commands_pending_addition;
    AtomicVar<uint32>                               m_num_draw_commands_pending_addition { 0 };
    Mutex                                           m_draw_commands_mutex;

    FixedArray<GPUBufferRef, max_frames_in_flight>  m_instance_buffers;
};

} // namespace hyperion

#endif