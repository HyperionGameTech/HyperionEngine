/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/Frustum.hpp>
#include <Core/Math/Color.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/Shared.hpp>

namespace Hyperion {

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

static constexpr int MaxDebugDrawShapeTypes = 10;

ENGINE_API extern uint32 GetRingIndex();

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

class ENGINE_API MeshDebugDrawShapeBase : public IDebugDrawShape
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

class ENGINE_API SphereDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    SphereDebugDrawShape(DebugDrawCommandList& list);

    virtual ~SphereDebugDrawShape() override = default;

    void operator()(const Vec3f& position, float radius, const Color& color);
    void operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class ENGINE_API AmbientProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    AmbientProbeDebugDrawShape(DebugDrawCommandList& list);

    virtual ~AmbientProbeDebugDrawShape() override = default;

    virtual void UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const override;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class ENGINE_API ReflectionProbeDebugDrawShape : public SphereDebugDrawShape
{
public:
    ReflectionProbeDebugDrawShape(DebugDrawCommandList& list);

    virtual ~ReflectionProbeDebugDrawShape() override = default;

    virtual void UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const override;

    void operator()(const Vec3f& position, float radius, const EnvProbe& envProbe);
};

class ENGINE_API BoxDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    BoxDebugDrawShape(DebugDrawCommandList& list);

    virtual ~BoxDebugDrawShape() override = default;

    virtual bool CheckShouldCull(DebugDrawCommand* cmd, const Frustum& frustum) const override;

    void operator()(const Vec3f& position, const Vec3f& size, const Color& color);
    void operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes);

    /*! \brief Draw an arbitrarily oriented box. The cube mesh is unit (corners at +/-1),
     *  so the scale component of \ref transform acts as the half-extent of the resulting box. */
    void operator()(const Transform& transform, const Color& color);
    void operator()(const Transform& transform, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class ENGINE_API CylinderDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    CylinderDebugDrawShape(DebugDrawCommandList& list);

    virtual ~CylinderDebugDrawShape() override = default;

    /*! \brief Draw a Y-axis aligned cylinder. The unit mesh has radius 1 and height 1,
     *  so \ref radius and \ref height map directly to the resulting dimensions. */
    void operator()(const Vec3f& position, float radius, float height, const Color& color);
    void operator()(const Vec3f& position, float radius, float height, const Color& color, const RenderableAttributeSet& attributes);

    /*! \brief Draw an arbitrarily oriented cylinder. The unit mesh has radius 1 and height 1,
     *  so the X/Z scale components act as the radius and the Y scale component as the height. */
    void operator()(const Transform& transform, const Color& color);
    void operator()(const Transform& transform, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class ENGINE_API PlaneDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    PlaneDebugDrawShape(DebugDrawCommandList& list);

    virtual ~PlaneDebugDrawShape() override = default;

    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color);
    void operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class ENGINE_API TriangleDebugDrawShape : public MeshDebugDrawShapeBase
{
public:
    TriangleDebugDrawShape(DebugDrawCommandList& list);

    virtual ~TriangleDebugDrawShape() override = default;

    void operator()(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const Color& color);
    void operator()(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const Color& color, const RenderableAttributeSet& attributes);

private:
    virtual Mesh* GetMesh_Internal() const override;
};

class DebugDrawCommandList final
{
public:
    friend class DebugDrawer;

    DebugDrawCommandList(DebugDrawer* debugDrawer);

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
    CylinderDebugDrawShape cylinder;
    PlaneDebugDrawShape plane;
    TriangleDebugDrawShape triangle;

private:
    DebugDrawer* m_debugDrawer;
    Array<DebugDrawCommandHeader> m_headers;
    ByteBuffer m_buffer;
    uint32 m_bufferOffset;
};

class DebugDrawer
{
    friend class DebugDrawCommandList;

public:
    static constexpr uint32 BufferCount = 3;

    static DebugDrawer& GetInstance();

    DebugDrawer();
    
    DebugDrawer(const DebugDrawer& other) = delete;
    DebugDrawer& operator=(const DebugDrawer& other) = delete;

    ~DebugDrawer();

    bool IsEnabled() const;

    HYP_FORCE_INLINE uint32 NumEnqueuedDrawCommands() const
    {
        return uint32(m_headers[m_renderIndex].Size());
    }

    void Initialize();
    void Shutdown();

    void Update();

    /*! \brief Take ownership of the slot the sim thread most recently finished, so the sim is free to
     *  swap its own slots again while this frame is still being drawn.
     *  Call from the render thread while the sim/render exclusive window is held. */
    void AcquireRenderCommands();

    void Render(Frame* frame, const RenderSetup& renderSetup);

    DebugDrawCommandList& CreateCommandList();

private:
    GraphicsPipelineRef FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 layerIndex, PassData* passData);
    void ClearCommands();

    FixedArray<Array<DebugDrawCommandHeader>, BufferCount> m_headers;
    FixedArray<ByteBuffer, BufferCount> m_buffers;
    FixedArray<uint32, BufferCount> m_bufferOffsets;

    uint32 m_pendingIndex = 0;
    uint32 m_readyIndex = 1;
    uint32 m_renderIndex = 2;

    // buffer sizes over the last X frames. we max() this to determine if we should compact the buffer
    FixedArray<size_t, 10> m_bufferSizeHistory;

    // Triple-buffered list of debug draw command lists.
    //  - the slot at m_pendingIndex accumulates per-caller command list objects
    //    - (CreateCommandList appends here from sim thread).
    //    - Update() swaps pending/ready
    //    - the render thread then swaps ready/render inside the sim/render exclusive window, so the sim can keep swapping
    //      its own two slots while a frame is still being drawn from m_renderIndex.
    FixedArray<List<DebugDrawCommandList>, BufferCount> m_commandLists;

    typedef Array<ImmediateDrawShaderData, RenderAllocator> CachedPartitionedShaderData[MaxDebugDrawShapeTypes];

    CachedPartitionedShaderData m_cachedPartitionedShaderData;
};

} // namespace Hyperion
