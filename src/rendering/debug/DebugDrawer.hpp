/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_IMMEDIATE_MODE_HPP
#define HYPERION_IMMEDIATE_MODE_HPP

#include <Constants.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/Handle.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Color.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <rendering/Buffers.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class RenderGroup;
class Mesh;
class IDebugDrawer;
class DebugDrawCommandList;
class IDebugDrawShape;
class UIObject;
class UIStage;
class FullScreenPass;

enum class DebugDrawType : int
{
    MESH = 0,
    UI
};

struct DebugDrawCommand
{
    IDebugDrawShape *shape;
    Matrix4         transform_matrix;
    Color           color;
};

class IDebugDrawCommandList
{
public:
    virtual ~IDebugDrawCommandList() = default;

    virtual void Push(UniquePtr<DebugDrawCommand> &&command) = 0;
    virtual void Commit() = 0;
};

class IDebugDrawShape
{
public:
    virtual ~IDebugDrawShape() = default;

    virtual DebugDrawType GetDebugDrawType() const = 0;
};

class UIDebugDrawShapeBase : public IDebugDrawShape
{
public:
    virtual ~UIDebugDrawShapeBase() override = default;

    virtual DebugDrawType GetDebugDrawType() const override final
    {
        return DebugDrawType::UI;
    }
};

class HYP_API MeshDebugDrawShapeBase : public IDebugDrawShape
{
public:
    MeshDebugDrawShapeBase(IDebugDrawCommandList &command_list, const Handle<Mesh> &mesh);

    virtual ~MeshDebugDrawShapeBase() override = default;

    virtual DebugDrawType GetDebugDrawType() const override
    {
        return DebugDrawType::MESH;
    }

    HYP_FORCE_INLINE const Handle<Mesh> &GetMesh() const
    {
        return m_mesh;
    }

protected:
    IDebugDrawCommandList   &m_command_list;
    Handle<Mesh>            m_mesh;
};

class HYP_API SphereDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    SphereDebugDrawShape(IDebugDrawCommandList &command_list);

    virtual ~SphereDebugDrawShape() override = default;

    void operator()(const Vec3f &position, float radius, const Color &color);
};

class HYP_API AmbientProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    AmbientProbeDebugDrawShape(IDebugDrawCommandList &command_list)
        : SphereDebugDrawShape(command_list)
    {
    }

    virtual ~AmbientProbeDebugDrawShape() override = default;

    void operator()(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id);
};

class HYP_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(IDebugDrawCommandList &command_list)
        : SphereDebugDrawShape(command_list)
    {
    }

    virtual ~ReflectionProbeDebugDrawShape() override = default;

    void operator()(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id);
};

class HYP_API BoxDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    BoxDebugDrawShape(IDebugDrawCommandList &command_list);

    virtual ~BoxDebugDrawShape() override = default;

    void operator()(const Vec3f &position, const Vec3f &size, const Color &color);
};

class HYP_API PlaneDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    PlaneDebugDrawShape(IDebugDrawCommandList &command_list);

    virtual ~PlaneDebugDrawShape() override = default;

    void operator()(const FixedArray<Vec3f, 4> &points, const Color &color);
};

class HYP_API Text2DDebugDrawShape : public UIDebugDrawShapeBase
{
public:
    Text2DDebugDrawShape(IDebugDrawCommandList &command_list);

    virtual ~Text2DDebugDrawShape() override = default;

    void operator()(const Vec2f &position, const String &text, const Color &color);

private:
    IDebugDrawCommandList   &m_command_list;
};

class DebugDrawCommandList final : public IDebugDrawCommandList
{
public:
    DebugDrawCommandList(IDebugDrawer &debug_drawer)
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

    virtual ~DebugDrawCommandList() override = default;

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_num_draw_commands.Get(MemoryOrder::ACQUIRE) == 0;
    }

    virtual void Push(UniquePtr<DebugDrawCommand> &&command) override;
    virtual void Commit() override;

    SphereDebugDrawShape                Sphere;
    AmbientProbeDebugDrawShape          AmbientProbe;
    ReflectionProbeDebugDrawShape       ReflectionProbe;
    BoxDebugDrawShape                   Box;
    PlaneDebugDrawShape                 Plane;

private:
    IDebugDrawer                        &m_debug_drawer;
    Array<UniquePtr<DebugDrawCommand>>  m_draw_commands;
    Mutex                               m_draw_commands_mutex;
    AtomicVar<uint32>                   m_num_draw_commands { 0 };
};

class IDebugDrawer
{
public:
    virtual ~IDebugDrawer() = default;

    virtual void Initialize() = 0;
    virtual void Update(GameCounter::TickUnit delta) = 0;
    virtual void Render(Frame *frame) = 0;
    virtual void CommitCommands(Array<UniquePtr<DebugDrawCommand>> &&commands) = 0;
};

class MeshDebugDrawer final : public IDebugDrawer
{
public:
    MeshDebugDrawer();
    virtual ~MeshDebugDrawer() override;

    virtual void Initialize() override;
    virtual void Update(GameCounter::TickUnit delta) override;
    virtual void Render(Frame *frame) override;

    UniquePtr<DebugDrawCommandList> CreateCommandList();
    virtual void CommitCommands(Array<UniquePtr<DebugDrawCommand>> &&commands) override;

private:
    void UpdateDrawCommands();

    DebugDrawCommandList                            m_default_command_list;
    AtomicVar<bool>                                 m_is_initialized;

public: // Shapes
    SphereDebugDrawShape                            &Sphere;
    AmbientProbeDebugDrawShape                      &AmbientProbe;
    ReflectionProbeDebugDrawShape                   &ReflectionProbe;
    BoxDebugDrawShape                               &Box;
    PlaneDebugDrawShape                             &Plane;

private:
    ShaderRef                                       m_shader;
    FramebufferRef                                  m_framebuffer;
    Handle<RenderGroup>                             m_render_group;

    Array<UniquePtr<DebugDrawCommand>>              m_draw_commands;
    Array<UniquePtr<DebugDrawCommand>>              m_draw_commands_pending_addition;
    AtomicVar<uint32>                               m_num_draw_commands_pending_addition;
    Mutex                                           m_draw_commands_mutex;

    FixedArray<GPUBufferRef, max_frames_in_flight>  m_instance_buffers;
};

class DebugDrawer final
{
public:
    DebugDrawer();
    ~DebugDrawer();

    void Initialize();
    void Update(GameCounter::TickUnit delta);
    void Render(Frame *frame);

private:
    UniquePtr<MeshDebugDrawer>                      m_mesh_debug_drawer;
    UniquePtr<FullScreenPass>                       m_full_screen_pass;
    AtomicVar<bool>                                 m_is_initialized;

public: // Shapes
    SphereDebugDrawShape                            &Sphere;
    AmbientProbeDebugDrawShape                      &AmbientProbe;
    ReflectionProbeDebugDrawShape                   &ReflectionProbe;
    BoxDebugDrawShape                               &Box;
    PlaneDebugDrawShape                             &Plane;
};

} // namespace hyperion

#endif