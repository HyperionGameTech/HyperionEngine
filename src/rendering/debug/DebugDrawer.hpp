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

#include <core/config/Config.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Color.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class RenderGroup;
class Mesh;
class DebugDrawer;
class DebugDrawCommandList;
class IDebugDrawShape;
class UIObject;
class UIStage;
struct RenderSetup;

HYP_STRUCT(ConfigName = "app", JSONPath = "rendering.debug.debug_drawer")
struct DebugDrawerConfig : public ConfigBase<DebugDrawerConfig>
{
    HYP_FIELD(Description = "Enable or disable the debug drawer.", JSONPath = "enabled")
    bool enabled = false;

    virtual ~DebugDrawerConfig() override = default;
};

enum class DebugDrawType : int
{
    MESH = 0
};

struct DebugDrawCommand
{
    IDebugDrawShape* shape;
    Matrix4 transform_matrix;
    Color color;
    RenderableAttributeSet attributes;
};

class IDebugDrawCommandList
{
public:
    virtual ~IDebugDrawCommandList() = default;

    virtual void Push(UniquePtr<DebugDrawCommand>&& command) = 0;
    virtual void Commit() = 0;
};

class IDebugDrawShape
{
public:
    virtual ~IDebugDrawShape() = default;

    virtual DebugDrawType GetDebugDrawType() const = 0;
};

class HYP_API MeshDebugDrawShapeBase : public IDebugDrawShape
{
public:
    MeshDebugDrawShapeBase(IDebugDrawCommandList& command_list, const Handle<Mesh>& mesh);

    virtual ~MeshDebugDrawShapeBase() override = default;

    virtual DebugDrawType GetDebugDrawType() const override
    {
        return DebugDrawType::MESH;
    }

    HYP_FORCE_INLINE const Handle<Mesh>& GetMesh() const
    {
        return m_mesh;
    }

protected:
    IDebugDrawCommandList& m_command_list;
    Handle<Mesh> m_mesh;
};

class HYP_API SphereDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    SphereDebugDrawShape(IDebugDrawCommandList& command_list);

    virtual ~SphereDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const Color& color);
    void operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes);
};

class HYP_API AmbientProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    AmbientProbeDebugDrawShape(IDebugDrawCommandList& command_list)
        : SphereDebugDrawShape(command_list)
    {
    }

    virtual ~AmbientProbeDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const EnvProbe& env_probe);
};

class HYP_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(IDebugDrawCommandList& command_list)
        : SphereDebugDrawShape(command_list)
    {
    }

    virtual ~ReflectionProbeDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const EnvProbe& env_probe);
};

class HYP_API BoxDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    BoxDebugDrawShape(IDebugDrawCommandList& command_list);

    virtual ~BoxDebugDrawShape() override = default;

    void operator()(const Vec3f& position, const Vec3f& size, const Color& color);
    void operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes);
};

class HYP_API PlaneDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    PlaneDebugDrawShape(IDebugDrawCommandList& command_list);

    virtual ~PlaneDebugDrawShape() override = default;

    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color);
    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes);
};

class DebugDrawCommandList final : public IDebugDrawCommandList
{
public:
    friend class DebugDrawer;

    DebugDrawCommandList(DebugDrawer& debug_drawer)
        : Sphere(*this),
          AmbientProbe(*this),
          ReflectionProbe(*this),
          Box(*this),
          Plane(*this),
          m_debug_drawer(debug_drawer)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList& other) = delete;
    DebugDrawCommandList& operator=(const DebugDrawCommandList& other) = delete;

    virtual ~DebugDrawCommandList() override = default;

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_num_draw_commands.Get(MemoryOrder::ACQUIRE) == 0;
    }

    virtual void Push(UniquePtr<DebugDrawCommand>&& command) override;
    virtual void Commit() override;

    SphereDebugDrawShape Sphere;
    AmbientProbeDebugDrawShape AmbientProbe;
    ReflectionProbeDebugDrawShape ReflectionProbe;
    BoxDebugDrawShape Box;
    PlaneDebugDrawShape Plane;

private:
    DebugDrawer& m_debug_drawer;
    Array<UniquePtr<DebugDrawCommand>> m_draw_commands;
    Mutex m_draw_commands_mutex;
    AtomicVar<uint32> m_num_draw_commands { 0 };
};

class IDebugDrawer
{
public:
    virtual ~IDebugDrawer() = default;

    virtual bool IsEnabled() const = 0;

    virtual void Initialize() = 0;
    virtual void Update(float delta) = 0;
    virtual void Render(FrameBase* frame, const RenderSetup& render_setup) = 0;
};

class HYP_API DebugDrawer final : public IDebugDrawer
{
public:
    DebugDrawer();
    virtual ~DebugDrawer() override;

    virtual bool IsEnabled() const override
    {
        return m_config.enabled;
    }

    virtual void Initialize() override;
    virtual void Update(float delta) override;
    virtual void Render(FrameBase* frame, const RenderSetup& render_setup) override;

    UniquePtr<DebugDrawCommandList> CreateCommandList();
    void CommitCommands(DebugDrawCommandList& command_list);

private:
    void UpdateDrawCommands();

    DebugDrawerConfig m_config;

    DebugDrawCommandList m_default_command_list;
    AtomicVar<bool> m_is_initialized;

public: // Shapes
    SphereDebugDrawShape& Sphere;
    AmbientProbeDebugDrawShape& AmbientProbe;
    ReflectionProbeDebugDrawShape& ReflectionProbe;
    BoxDebugDrawShape& Box;
    PlaneDebugDrawShape& Plane;

private:
    GraphicsPipelineRef FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawable_layer);

    ShaderRef m_shader;
    DescriptorTableRef m_descriptor_table;
    HashMap<RenderableAttributeSet, GraphicsPipelineWeakRef> m_graphics_pipelines;

    Array<UniquePtr<DebugDrawCommand>> m_draw_commands;
    Array<UniquePtr<DebugDrawCommand>> m_draw_commands_pending_addition;
    AtomicVar<uint32> m_num_draw_commands_pending_addition;
    Mutex m_draw_commands_mutex;

    FixedArray<GPUBufferRef, max_frames_in_flight> m_instance_buffers;
};

} // namespace hyperion

#endif