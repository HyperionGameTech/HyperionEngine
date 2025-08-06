/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Constants.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/Handle.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/config/Config.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Frustum.hpp>
#include <core/math/Color.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

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
class PassData;
struct RenderSetup;
struct ImmediateDrawShaderData;

HYP_STRUCT(ConfigName = "app", JsonPath = "rendering.debug.debugDrawer")
struct DebugDrawerConfig : public ConfigBase<DebugDrawerConfig>
{
    HYP_FIELD(Description = "Enable or disable the debug drawer.", JsonPath = "enabled")
    bool enabled = true;

    virtual ~DebugDrawerConfig() override = default;
};

enum class DebugDrawType : int
{
    MESH = 0
};

struct DebugDrawCommand
{
    IDebugDrawShape* shape;
    Matrix4 transformMatrix;
    Color color;
    RenderableAttributeSet attributes;
};

struct DebugDrawCommandHeader
{
    uint32 offset;
    uint32 size;

    void (*moveFn)(void* dst, void* src);
    void (*destructFn)(void*);
};

class IDebugDrawShape
{
public:
    virtual ~IDebugDrawShape() = default;

    virtual DebugDrawType GetDebugDrawType() const = 0;

    virtual bool CheckShouldCull(DebugDrawCommand* cmd, const Frustum& frustum) const
    {
        return false;
    }

    virtual void UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const;
};

class HYP_API MeshDebugDrawShapeBase : public IDebugDrawShape
{
public:
    MeshDebugDrawShapeBase(DebugDrawCommandList& list);

    virtual ~MeshDebugDrawShapeBase() override = default;

    virtual DebugDrawType GetDebugDrawType() const override
    {
        return DebugDrawType::MESH;
    }

    HYP_FORCE_INLINE Mesh* GetMesh() const
    {
        if (!m_mesh)
        {
            m_mesh = GetMesh_Internal();
        }

        return m_mesh;
    }

protected:
    virtual Mesh* GetMesh_Internal() const = 0;

    DebugDrawCommandList& list;

private:
    mutable Mesh* m_mesh;
};

class HYP_API SphereDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    SphereDebugDrawShape(DebugDrawCommandList& list);

    virtual ~SphereDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const Color& color);
    void operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class HYP_API AmbientProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    AmbientProbeDebugDrawShape(DebugDrawCommandList& list)
        : SphereDebugDrawShape(list)
    {
    }

    virtual ~AmbientProbeDebugDrawShape() override = default;

    virtual void UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const override;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class HYP_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(DebugDrawCommandList& list)
        : SphereDebugDrawShape(list)
    {
    }

    virtual ~ReflectionProbeDebugDrawShape() override = default;

    virtual void UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const override;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class HYP_API BoxDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    BoxDebugDrawShape(DebugDrawCommandList& list);

    virtual ~BoxDebugDrawShape() override = default;

    virtual bool CheckShouldCull(DebugDrawCommand* cmd, const Frustum& frustum) const override;

    void operator()(const Vec3f& position, const Vec3f& size, const Color& color);
    void operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class HYP_API PlaneDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    PlaneDebugDrawShape(DebugDrawCommandList& list);

    virtual ~PlaneDebugDrawShape() override = default;

    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color);
    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class DebugDrawCommandList final
{
public:
    friend class DebugDrawer;

    DebugDrawCommandList(DebugDrawer* debugDrawer)
        : m_debugDrawer(debugDrawer),
          sphere(*this),
          ambientProbe(*this),
          reflectionProbe(*this),
          box(*this),
          plane(*this),
          m_bufferOffset(0)
    {
    }

    DebugDrawCommandList(const DebugDrawCommandList& other) = delete;
    DebugDrawCommandList& operator=(const DebugDrawCommandList& other) = delete;

    DebugDrawCommandList(DebugDrawCommandList&& other) noexcept = delete;
    DebugDrawCommandList& operator=(DebugDrawCommandList&& other) noexcept = delete;

    ~DebugDrawCommandList();

    HYP_FORCE_INLINE DebugDrawer* GetDebugDrawer() const
    {
        return m_debugDrawer;
    }

    void* Alloc(uint32 size, uint32 alignment, DebugDrawCommandHeader& outHeader);
    void Push(const DebugDrawCommandHeader& header);

    SphereDebugDrawShape sphere;
    AmbientProbeDebugDrawShape ambientProbe;
    ReflectionProbeDebugDrawShape reflectionProbe;
    BoxDebugDrawShape box;
    PlaneDebugDrawShape plane;

private:
    DebugDrawer* m_debugDrawer;
    Array<DebugDrawCommandHeader> m_headers;
    ByteBuffer m_buffer;
    uint32 m_bufferOffset;
};

class HYP_API DebugDrawer
{
public:
    DebugDrawer();
    ~DebugDrawer();

    HYP_FORCE_INLINE bool IsEnabled() const
    {
        return m_config.enabled;
    }

    void Initialize();
    void Update(float delta);
    void Render(FrameBase* frame, const RenderSetup& renderSetup);

    DebugDrawCommandList& CreateCommandList();

private:
    DebugDrawerConfig m_config;

    AtomicVar<bool> m_isInitialized;

private:
    GraphicsPipelineRef FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawableLayer, PassData* passData);

    void ClearCommands(uint32 idx);

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;
    HashMap<RenderableAttributeSet, GraphicsPipelineWeakRef> m_graphicsPipelines;

    FixedArray<Array<DebugDrawCommandHeader>, g_tripleBuffer ? 3 : 2> m_headers;
    FixedArray<ByteBuffer, g_tripleBuffer ? 3 : 2> m_buffers;
    FixedArray<uint32, g_tripleBuffer ? 3 : 2> m_bufferOffsets;

    // buffer sizes over the last X frames. we max() this to determine if we should compact the buffer
    FixedArray<SizeType, 10> m_bufferSizeHistory;

    FixedArray<LinkedList<DebugDrawCommandList>, g_tripleBuffer ? 3 : 2> m_commandLists;

    FixedArray<GpuBufferRef, g_framesInFlight> m_instanceBuffers;

    using CachedPartitionedShaderData = HashMap<IDebugDrawShape*, Array<ImmediateDrawShaderData, DynamicAllocator>, HashTable_DynamicNodeAllocator<KeyValuePair<IDebugDrawShape*, Array<ImmediateDrawShaderData, DynamicAllocator>>>>;
    CachedPartitionedShaderData m_cachedPartitionedShaderData;
};

} // namespace hyperion
