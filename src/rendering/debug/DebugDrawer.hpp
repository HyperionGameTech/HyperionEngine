/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Constants.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/reflection/Handle.hpp>
#include <core/reflection/HypObjectBase.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/config/Config.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Frustum.hpp>
#include <core/math/Color.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shared.hpp>

#include <core/Types.hpp>

namespace hyperion {

class RenderGroup;
class Mesh;
class EnvProbe;
class DebugDrawer;
struct DebugDrawCommand;
class DebugDrawCommandList;
class IDebugDrawShape;
class UIObject;
class UIStage;
class PassData;
struct RenderSetup;
struct ImmediateDrawShaderData;

static constexpr int MaxDebugDrawShapeTypes = 8;

HYP_STRUCT(ConfigName = "GlobalConfig", JsonPath = "Rendering.Debug.DebugDrawer")
struct DebugDrawerConfig : public ConfigBase<DebugDrawerConfig>
{
    HYP_STRUCT_BODY(DebugDrawerConfig);

    HYP_FIELD(Description = "Enable or disable the debug drawer.")
    bool enabled = true;

    virtual ~DebugDrawerConfig() override = default;
};

enum class DebugDrawType : int
{
    MESH = 0
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

    int shapeId = -1;
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
    AmbientProbeDebugDrawShape(DebugDrawCommandList& list);

    virtual ~AmbientProbeDebugDrawShape() override = default;

    virtual void UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const override;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class HYP_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(DebugDrawCommandList& list);

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

HYP_CLASS()
class HYP_API DebugDrawer final : public HypObjectBase
{
    HYP_OBJECT_BODY(DebugDrawer);

public:
    DebugDrawer();
    ~DebugDrawer() override;

    HYP_FORCE_INLINE bool IsEnabled() const
    {
        return m_config.enabled;
    }

    HYP_FORCE_INLINE const DebugDrawerConfig& GetConfig() const
    {
        return m_config;
    }

    void Update(float delta);
    void Render(FrameBase* frame, const RenderSetup& renderSetup);

    DebugDrawCommandList& CreateCommandList();

private:
    void Init() override;

    DebugDrawerConfig m_config;

    AtomicVar<bool> m_isInitialized;

private:
    GraphicsPipelineRef FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawableLayer, PassData* passData);

    void ClearCommands(uint32 idx);

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;
    HashMap<RenderableAttributeSet, GraphicsPipelineCacheHandle> m_graphicsPipelines;

    FixedArray<Array<DebugDrawCommandHeader>, RingBufferDepth> m_headers;
    FixedArray<ByteBuffer, RingBufferDepth> m_buffers;
    FixedArray<uint32, RingBufferDepth> m_bufferOffsets;

    // buffer sizes over the last X frames. we max() this to determine if we should compact the buffer
    FixedArray<SizeType, 10> m_bufferSizeHistory;

    FixedArray<LinkedList<DebugDrawCommandList>, RingBufferDepth> m_commandLists;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_instanceBuffers;

    typedef Array<ImmediateDrawShaderData, DynamicAllocator> CachedPartitionedShaderData[MaxDebugDrawShapeTypes];

    CachedPartitionedShaderData m_cachedPartitionedShaderData;
};

} // namespace hyperion
