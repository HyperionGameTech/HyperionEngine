/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderStats.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/Mesh.hpp>

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <util/MeshBuilder.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

static RenderableAttributeSet GetRenderableAttributes()
{
    return RenderableAttributeSet(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes,
            .topology = TOP_LINES },
        MaterialAttributes {
            .bucket = RB_TRANSLUCENT,
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction::None(),
            .flags = MAF_DEPTH_TEST,
            .stencilFunction = StencilFunction {
                .passOp = SO_KEEP,
                .failOp = SO_KEEP,
                .depthFailOp = SO_KEEP,
                .compareOp = SCO_NOT_EQUAL,
                .mask = 0x0,
                .value = 0x1 } });
}

#pragma region DebugDrawCommand_Probe

struct DebugDrawCommand_Probe : DebugDrawCommand
{
    const EnvProbe* envProbe = nullptr;
};

#pragma endregion DebugDrawCommand_Probe

#pragma region IDebugDrawShape

void IDebugDrawShape::UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const
{
    *bufferData = ImmediateDrawShaderData {
        cmd->transformMatrix,
        cmd->color.Packed()
    };
}

#pragma endregion IDebugDrawShape

#pragma region MeshDebugDrawShapeBase

MeshDebugDrawShapeBase::MeshDebugDrawShapeBase(DebugDrawCommandList& list)
    : list(list),
      m_mesh(nullptr)
{
}

#pragma endregion MeshDebugDrawShapeBase

#pragma region SphereDebugDrawShape

// Global counter that is incremented the first time a debug draw shape class is constructed.
// This number is assigned to each debug draw shape type so we can use it to cache render data on a
// per-shape basis. It's not meant to be a stable ID but just needs to be unique
static int g_debugDrawShapeIdCounter = 0;

static inline int NextShapeId()
{
    int shapeId = g_debugDrawShapeIdCounter++;
    AssertDebug(shapeId < g_maxDebugDrawShapeTypes);
    
    return shapeId;
}

SphereDebugDrawShape::SphereDebugDrawShape(DebugDrawCommandList& list)
    : MeshDebugDrawShapeBase(list)
{
    static const int s_shapeId = NextShapeId();
    shapeId = s_shapeId;
    
    (void)GetMesh(); // hack to preload mesh so it doesn't try to load during render pass
}

Mesh* SphereDebugDrawShape::GetMesh_Internal() const
{
    static struct MeshInitializer
    {
        Handle<Mesh> mesh;

        MeshInitializer()
        {
            mesh = MeshBuilder::NormalizedCubeSphere(4);
            mesh->SetName(NAME("SphereDebugDrawShape"));

            g_assetManager->GetAssetRegistry()->RegisterAsset("$Engine/Media/Meshes", mesh->GetAsset());

            InitObject(mesh);
        }
    } initializer;

    return initializer.mesh;
}

void SphereDebugDrawShape::operator()(const Vec3f& position, float radius, const Color& color)
{
    (*this)(position, radius, color, GetRenderableAttributes());
}

void SphereDebugDrawShape::operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes)
{
    if (!list.GetDebugDrawer()->IsEnabled())
    {
        return;
    }

    DebugDrawCommandHeader header;

    DebugDrawCommand* ptr = reinterpret_cast<DebugDrawCommand*>(list.Alloc(sizeof(DebugDrawCommand), alignof(DebugDrawCommand), header));
    new (ptr) DebugDrawCommand {
        this,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        color,
        attributes
    };

    header.destructFn = &Memory::Destruct<DebugDrawCommand>;
    header.moveFn = [](void* dst, void* src)
    {
        new (dst) DebugDrawCommand(std::move(*reinterpret_cast<DebugDrawCommand*>(src)));
    };

    list.Push(header);
}

#pragma endregion SphereDebugDrawShape

#pragma region AmbientProbeDebugDrawShape

AmbientProbeDebugDrawShape::AmbientProbeDebugDrawShape(DebugDrawCommandList& list)
    : SphereDebugDrawShape(list)
{
    static const int s_shapeId = NextShapeId();
    shapeId = s_shapeId;
}

void AmbientProbeDebugDrawShape::UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const
{
    IDebugDrawShape::UpdateBufferData(cmd, bufferData);

    const uint32 envProbeIndex = RenderApi_RetrieveResourceBinding(static_cast<DebugDrawCommand_Probe*>(cmd)->envProbe);

    bufferData->envProbeType = EPT_AMBIENT;
    bufferData->envProbeIndex = envProbeIndex;
}

void AmbientProbeDebugDrawShape::operator()(const Vec3f& position, float radius, const EnvProbe& envProbe)
{
    if (!list.GetDebugDrawer()->IsEnabled())
    {
        return;
    }

    Assert(envProbe.IsReady());

    DebugDrawCommandHeader header;

    DebugDrawCommand_Probe* ptr = reinterpret_cast<DebugDrawCommand_Probe*>(list.Alloc(sizeof(DebugDrawCommand_Probe), alignof(DebugDrawCommand_Probe), header));

    new (ptr) DebugDrawCommand();
    ptr->shape = this;
    ptr->transformMatrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    ptr->color = Color::White();
    ptr->envProbe = &envProbe;

    header.destructFn = &Memory::Destruct<DebugDrawCommand_Probe>;
    header.moveFn = [](void* dst, void* src)
    {
        new (dst) DebugDrawCommand_Probe(std::move(*reinterpret_cast<DebugDrawCommand_Probe*>(src)));
    };

    list.Push(header);
}

#pragma endregion AmbientProbeDebugDrawShape

#pragma region ReflectionProbeDebugDrawShape

ReflectionProbeDebugDrawShape::ReflectionProbeDebugDrawShape(DebugDrawCommandList& list)
    : SphereDebugDrawShape(list)
{
    static const int s_shapeId = NextShapeId();
    shapeId = s_shapeId;
}

void ReflectionProbeDebugDrawShape::UpdateBufferData(DebugDrawCommand* cmd, ImmediateDrawShaderData* bufferData) const
{
    IDebugDrawShape::UpdateBufferData(cmd, bufferData);

    const uint32 envProbeIndex = RenderApi_RetrieveResourceBinding(static_cast<DebugDrawCommand_Probe*>(cmd)->envProbe);

    bufferData->envProbeType = EPT_REFLECTION;
    bufferData->envProbeIndex = envProbeIndex;
}

void ReflectionProbeDebugDrawShape::operator()(const Vec3f& position, float radius, const EnvProbe& envProbe)
{
    if (!list.GetDebugDrawer()->IsEnabled())
    {
        return;
    }

    Assert(envProbe.IsReady());

    DebugDrawCommandHeader header;

    DebugDrawCommand_Probe* ptr = reinterpret_cast<DebugDrawCommand_Probe*>(list.Alloc(sizeof(DebugDrawCommand_Probe), alignof(DebugDrawCommand_Probe), header));

    new (ptr) DebugDrawCommand();
    ptr->shape = this;
    ptr->transformMatrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    ptr->color = Color::White();
    ptr->envProbe = &envProbe;

    header.destructFn = &Memory::Destruct<DebugDrawCommand_Probe>;
    header.moveFn = [](void* dst, void* src)
    {
        new (dst) DebugDrawCommand_Probe(std::move(*reinterpret_cast<DebugDrawCommand_Probe*>(src)));
    };

    list.Push(header);
}

#pragma endregion ReflectionProbeDebugDrawShape

#pragma region BoxDebugDrawShape

BoxDebugDrawShape::BoxDebugDrawShape(DebugDrawCommandList& list)
    : MeshDebugDrawShapeBase(list)
{
    static const int s_shapeId = NextShapeId();
    shapeId = s_shapeId;
    
    (void)GetMesh(); // hack to preload mesh so it doesn't try to load during render pass
}

bool BoxDebugDrawShape::CheckShouldCull(DebugDrawCommand* cmd, const Frustum& frustum) const
{
    const BoundingBox aabb = cmd->transformMatrix * BoundingBox(Vec3f(-1.0f), Vec3f(1.0f));

    return !frustum.ContainsAABB(aabb);
}

Mesh* BoxDebugDrawShape::GetMesh_Internal() const
{
    static struct MeshInitializer
    {
        Handle<Mesh> mesh;

        MeshInitializer()
        {
            mesh = MeshBuilder::Cube();
            mesh->SetName(NAME("BoxDebugDrawShape"));

            g_assetManager->GetAssetRegistry()->RegisterAsset("$Engine/Media/Meshes", mesh->GetAsset());

            InitObject(mesh);
        }
    } initializer;

    return initializer.mesh;
}

void BoxDebugDrawShape::operator()(const Vec3f& position, const Vec3f& size, const Color& color)
{
    (*this)(position, size, color, GetRenderableAttributes());
}

void BoxDebugDrawShape::operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes)
{
    if (!list.GetDebugDrawer()->IsEnabled())
    {
        return;
    }

    DebugDrawCommandHeader header;

    DebugDrawCommand* ptr = reinterpret_cast<DebugDrawCommand*>(list.Alloc(sizeof(DebugDrawCommand), alignof(DebugDrawCommand), header));
    new (ptr) DebugDrawCommand {
        this,
        Transform(position, size, Quaternion::Identity()).GetMatrix(),
        color,
        attributes
    };

    header.destructFn = &Memory::Destruct<DebugDrawCommand>;
    header.moveFn = [](void* dst, void* src)
    {
        new (dst) DebugDrawCommand(std::move(*reinterpret_cast<DebugDrawCommand*>(src)));
    };

    list.Push(header);
}

#pragma endregion BoxDebugDrawShape

#pragma region PlaneDebugDrawShape

PlaneDebugDrawShape::PlaneDebugDrawShape(DebugDrawCommandList& list)
    : MeshDebugDrawShapeBase(list)
{
    static const int s_shapeId = NextShapeId();
    shapeId = s_shapeId;
    
    (void)GetMesh(); // hack to preload mesh so it doesn't try to load during render pass
}

Mesh* PlaneDebugDrawShape::GetMesh_Internal() const
{
    static struct MeshInitializer
    {
        Handle<Mesh> mesh;

        MeshInitializer()
        {
            mesh = MeshBuilder::Quad();
            mesh->SetName(NAME("PlaneDebugDrawShape"));

            g_assetManager->GetAssetRegistry()->RegisterAsset("$Engine/Media/Meshes", mesh->GetAsset());

            InitObject(mesh);
        }
    } initializer;

    return initializer.mesh;
}

void PlaneDebugDrawShape::operator()(const FixedArray<Vec3f, 4>& points, const Color& color)
{
    (*this)(points, color, GetRenderableAttributes());
}

void PlaneDebugDrawShape::operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes)
{
    if (!list.GetDebugDrawer()->IsEnabled())
    {
        return;
    }

    Vec3f x = (points[1] - points[0]).Normalize();
    Vec3f y = (points[2] - points[0]).Normalize();
    Vec3f z = x.Cross(y).Normalize();

    const Vec3f center = points.Avg();

    Matrix4 transformMatrix;
    transformMatrix.rows[0] = Vec4f(x, 0.0f);
    transformMatrix.rows[1] = Vec4f(y, 0.0f);
    transformMatrix.rows[2] = Vec4f(z, 0.0f);
    transformMatrix.rows[3] = Vec4f(center, 1.0f);

    DebugDrawCommandHeader header;

    DebugDrawCommand* ptr = reinterpret_cast<DebugDrawCommand*>(list.Alloc(sizeof(DebugDrawCommand), alignof(DebugDrawCommand), header));
    new (ptr) DebugDrawCommand {
        this,
        transformMatrix,
        color,
        attributes
    };

    header.destructFn = &Memory::Destruct<DebugDrawCommand>;
    header.moveFn = [](void* dst, void* src)
    {
        new (dst) DebugDrawCommand(std::move(*reinterpret_cast<DebugDrawCommand*>(src)));
    };

    list.Push(header);
}

#pragma endregion PlaneDebugDrawShape

#pragma region DebugDrawer

DebugDrawer::DebugDrawer()
    : m_config(DebugDrawerConfig::FromConfig()),
      m_bufferOffsets {},
      m_bufferSizeHistory {},
      m_isInitialized(false)
{
}

DebugDrawer::~DebugDrawer()
{
    for (uint32 i = 0; i < uint32(m_commandLists.Size()); i++)
    {
        m_commandLists[i].Clear();
    }

    for (uint32 i = 0; i < uint32(m_buffers.Size()); i++)
    {
        ClearCommands(i);
    }

    m_shader.Reset();

    SafeDelete(std::move(m_instanceBuffers));
    SafeDelete(std::move(m_descriptorTable));
}

void DebugDrawer::Initialize()
{
    HYP_SCOPE;

    Assert(!m_isInitialized.Get(MemoryOrder::ACQUIRE));

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_instanceBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(ImmediateDrawShaderData));
        m_instanceBuffers[frameIndex]->SetDebugName(NAME_FMT("DebugDrawer_ImmediateDrawsBuffer_{}", frameIndex));
        m_instanceBuffers[frameIndex]->SetRequireCpuAccessible(true); // TEMP
        DeferCreate(m_instanceBuffers[frameIndex]);
    }

    m_shader = g_shaderManager->GetOrCreate(
        NAME("DebugAABB"),
        ShaderProperties(staticMeshVertexAttributes, { { NAME("IMMEDIATE_MODE") } }));

    Assert(m_shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    AssertDebug(m_descriptorTable == nullptr);

    m_descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
    Assert(m_descriptorTable != nullptr);

    const uint32 debugDrawerDescriptorSetIndex = m_descriptorTable->GetDescriptorSetIndex("DebugDrawerDescriptorSet");
    Assert(debugDrawerDescriptorSetIndex != ~0u);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& debugDrawerDescriptorSet = m_descriptorTable->GetDescriptorSet(debugDrawerDescriptorSetIndex, frameIndex);
        Assert(debugDrawerDescriptorSet != nullptr);

        debugDrawerDescriptorSet->SetElement("ImmediateDrawsBuffer", m_instanceBuffers[frameIndex]);
    }

    DeferCreate(m_descriptorTable);

    m_isInitialized.Set(true, MemoryOrder::RELEASE);
}

void DebugDrawer::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    const uint32 idx = RenderApi_GetFrameIndex();

    if (m_commandLists[idx].Empty())
    {
        return;
    }

    ByteBuffer& buffer = m_buffers[idx];
    uint32& bufferOffset = m_bufferOffsets[idx];

    for (DebugDrawCommandList& it : m_commandLists[idx])
    {
        if (it.m_bufferOffset == 0)
        {
            // no data in buffers, skip
            continue;
        }

        // concat all command lists and take ownership of the data
        for (DebugDrawCommandHeader& header : it.m_headers)
        {
            const uint32 newAlignedOffset = ByteUtil::AlignAs(bufferOffset, 16);

            void* vp = it.m_buffer.Data() + header.offset;

            if (buffer.Size() < newAlignedOffset + header.size)
            {
                ByteBuffer newBuffer;
                newBuffer.SetSize(MathUtil::Ceil<double, SizeType>((newAlignedOffset + header.size) * 1.5));

                // have to move all current commands since the buffer will realloc
                for (DebugDrawCommandHeader& currHeader : m_headers[idx])
                {
                    if (currHeader.moveFn)
                    {
                        currHeader.moveFn(reinterpret_cast<void*>(newBuffer.Data() + currHeader.offset), reinterpret_cast<void*>(buffer.Data() + currHeader.offset));
                    }
                    else
                    {
                        Memory::MemCpy(newBuffer.Data() + currHeader.offset, buffer.Data() + currHeader.offset, currHeader.size);
                    }

                    if (currHeader.destructFn)
                    {
                        currHeader.destructFn(reinterpret_cast<void*>(buffer.Data() + currHeader.offset));
                    }
                }

                buffer = std::move(newBuffer);
            }

            if (header.moveFn)
            {
                header.moveFn(reinterpret_cast<void*>(buffer.Data() + newAlignedOffset), vp);
            }
            else
            {
                Memory::MemCpy(buffer.Data() + newAlignedOffset, vp, header.size);
            }

            if (header.destructFn)
            {
                header.destructFn(vp);
            }

            header.offset = newAlignedOffset;

            bufferOffset = newAlignedOffset + header.size;

            m_headers[idx].PushBack(header);
        }

        it.m_headers.Clear();
        it.m_buffer = ByteBuffer();
        it.m_bufferOffset = 0;
    }
}

void DebugDrawer::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // wait for initialization on the game thread
    if (!m_isInitialized.Get(MemoryOrder::RELAXED))
    {
        return;
    }

    const uint32 idx = RenderApi_GetFrameIndex();

    if (!IsEnabled() || m_headers[idx].Empty())
    {
        // clear, otherwise we'll start to leak a huge amount of memory
        ClearCommands(idx);

        return;
    }

    Assert(renderSetup.HasView());

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi_GetRenderProxy(renderSetup.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    GpuBufferRef& instanceBuffer = m_instanceBuffers[frameIndex];
    bool wasInstanceBufferRebuilt = false;

    if (m_headers[idx].Size() * sizeof(ImmediateDrawShaderData) > instanceBuffer->Size())
    {
        HYP_GFX_ASSERT(instanceBuffer->EnsureCapacity(
            m_headers[idx].Size() * sizeof(ImmediateDrawShaderData),
            &wasInstanceBufferRebuilt));
    }

    const uint32 debugDrawerDescriptorSetIndex = m_descriptorTable->GetDescriptorSetIndex("DebugDrawerDescriptorSet");
    Assert(debugDrawerDescriptorSetIndex != ~0u);

    const DescriptorSetRef& debugDrawerDescriptorSet = m_descriptorTable->GetDescriptorSet(debugDrawerDescriptorSetIndex, frameIndex);
    Assert(debugDrawerDescriptorSet != nullptr);

    // Update descriptor set if instance buffer was rebuilt
    if (wasInstanceBufferRebuilt)
    {
        debugDrawerDescriptorSet->SetElement("ImmediateDrawsBuffer", instanceBuffer);
    }

    auto& partitionedShaderData = m_cachedPartitionedShaderData;

    for (auto& it : partitionedShaderData)
    {
        // don't want to keep filling up buffers
        it.Clear();
    }

    // @NOTE: Don't use of list-dependent stuff on any of the shapes in currShapes.
    // It's simply here because it's faster to cache the pointers to each shape by its shape id,
    // and we're only using stuff that doesn't use the shape's debug draw list at all.
    // if we want to use more stuff on each shape, we'll need to devise a different solutoin.
    IDebugDrawShape* currShapes[g_maxDebugDrawShapeTypes] { nullptr };
    
    for (SizeType drawCommandIdx = 0; drawCommandIdx < m_headers[idx].Size(); drawCommandIdx++)
    {
        uint32 offset = m_headers[idx][drawCommandIdx].offset;
        uint32 size = m_headers[idx][drawCommandIdx].size;
        AssertDebug(offset + size <= m_buffers[idx].Size());

        DebugDrawCommand* drawCommand = reinterpret_cast<DebugDrawCommand*>(m_buffers[idx].Data() + offset);

        uint32 envProbeType = uint32(EPT_INVALID);
        uint32 envProbeIndex = ~0u;

        if (drawCommand->shape->CheckShouldCull(drawCommand, cameraProxy->viewFrustum))
        {
            continue;
        }
        
        const int shapeIdx = drawCommand->shape->shapeId;
        
        AssertDebug(shapeIdx >= 0 && shapeIdx < g_maxDebugDrawShapeTypes);
        
        auto& shaderData = partitionedShaderData[shapeIdx];
        currShapes[shapeIdx] = drawCommand->shape;
        
        ImmediateDrawShaderData& shaderDataElement = shaderData.EmplaceBack();

        drawCommand->shape->UpdateBufferData(drawCommand, &shaderDataElement);

        shaderDataElement.idx = (uint32)drawCommandIdx;
    }

    struct
    {
        HashCode attributesHashCode;
        GraphicsPipelineRef graphicsPipeline;
        uint32 drawableLayer = ~0u;
    } previousState;

    SizeType shaderDataOffset = 0;
    SizeType totalDrawCalls = 0;
    SizeType totalInstancedDraws = 0;

#ifdef HYP_ENABLE_RENDER_STATS
    RenderStatsCounts counts {};
#endif

    for (uint32 shapeIdx = 0; shapeIdx < HYP_ARRAY_SIZE(partitionedShaderData); shapeIdx++)
    {
        auto& shaderData = partitionedShaderData[shapeIdx];
        
        if (shaderData.Empty())
        {
            continue;
        }

        Assert(instanceBuffer->Size() >= (shaderData.Size() + shaderDataOffset) * sizeof(ImmediateDrawShaderData));
        instanceBuffer->Copy(shaderDataOffset * sizeof(ImmediateDrawShaderData), shaderData.Size() * sizeof(ImmediateDrawShaderData), shaderData.Data());

        SizeType numToDraw = 0;

        auto commitCurrentDraws = [&]()
        {
            if (numToDraw != 0)
            {
#ifdef HYP_ENABLE_RENDER_STATS
                counts[ERS_DEBUG_DRAWS] += numToDraw;
#endif
                IDebugDrawShape* shape = currShapes[shapeIdx];
                AssertDebug(shape != nullptr);

                switch (shape->GetDebugDrawType())
                {
                case DebugDrawType::MESH:
                {
                    MeshDebugDrawShapeBase* meshShape = static_cast<MeshDebugDrawShapeBase*>(shape);

                    Mesh* mesh = meshShape->GetMesh();
                    AssertDebug(mesh && mesh->IsReady());

                    frame->renderQueue << BindVertexBuffer(mesh->GetVertexBuffer());
                    frame->renderQueue << BindIndexBuffer(mesh->GetIndexBuffer());
                    frame->renderQueue << DrawIndexed(mesh->NumIndices(), uint32(numToDraw));

                    ++totalDrawCalls;
                    totalInstancedDraws += numToDraw;

                    break;
                }
                default:
                    HYP_UNREACHABLE();
                }

                numToDraw = 0;
            }
        };

        for (SizeType i = 0; i < shaderData.Size(); i++)
        {
            AssertDebug(shaderData[i].idx != ~0u, "culled element should not be in array");

            const uint32 drawCommandIdx = shaderData[i].idx;

            uint32 offset = m_headers[idx][drawCommandIdx].offset;
            uint32 size = m_headers[idx][drawCommandIdx].size;
            AssertDebug(offset + size <= m_buffers[idx].Size());

            DebugDrawCommand* drawCommand = reinterpret_cast<DebugDrawCommand*>(m_buffers[idx].Data() + offset);

            bool isNewGraphicsPipeline = false;

            GraphicsPipelineRef& graphicsPipeline = previousState.graphicsPipeline;

            if (!graphicsPipeline.IsValid() || previousState.attributesHashCode != drawCommand->attributes.GetHashCode())
            {
                graphicsPipeline = FetchGraphicsPipeline(drawCommand->attributes, ++previousState.drawableLayer, renderSetup.passData);
                previousState.attributesHashCode = drawCommand->attributes.GetHashCode();

                isNewGraphicsPipeline = true;
            }

            if (isNewGraphicsPipeline)
            {
                // new graphics pipeline, commit current draws then bind new pipeline to keep adding draws
                commitCurrentDraws();

                frame->renderQueue << BindGraphicsPipeline(graphicsPipeline);

                frame->renderQueue << BindDescriptorTable(
                    m_descriptorTable,
                    graphicsPipeline,
                    { { "DebugDrawerDescriptorSet",
                          { { "ImmediateDrawsBuffer", ShaderDataOffset<ImmediateDrawShaderData>(uint32(shaderDataOffset)) } } },
                        { "Global",
                            { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                                { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) } } },
                        { "Object", {} } },
                    frameIndex);
            }

            shaderDataOffset++;
            numToDraw++;
        }

        commitCurrentDraws();
    }

#ifdef HYP_ENABLE_RENDER_STATS
    RenderApi_AddRenderStats(counts);
#endif

    ClearCommands(idx);
}

void DebugDrawer::ClearCommands(uint32 idx)
{
    HYP_SCOPE;
    AssertDebug(idx < m_commandLists.Size());

    for (DebugDrawCommandHeader& header : m_headers[idx])
    {
        if (header.destructFn)
        {
            header.destructFn(reinterpret_cast<void*>(m_buffers[idx].Data() + header.offset));
        }
    }

    // update buffer capacity based on history so we don't keep memory around longer than we need to.
    SizeType maxHistorySize = 0;

    for (int i = int(m_bufferSizeHistory.Size()) - 1; i >= 0; i--)
    {
        maxHistorySize = MathUtil::Max(maxHistorySize, m_bufferSizeHistory[i]);

        if (i > 0)
        {
            m_bufferSizeHistory[i] = m_bufferSizeHistory[i - 1];
        }
    }

    m_headers[idx].Clear();
    m_buffers[idx].SetSize(0);
    m_buffers[idx].SetCapacity(maxHistorySize);
    m_bufferOffsets[idx] = 0;

    m_bufferSizeHistory[0] = m_buffers[idx].Size();

    // safe to clear command lists only after rendering
    m_commandLists[idx].Clear();
}

DebugDrawCommandList& DebugDrawer::CreateCommandList()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    const uint32 idx = RenderApi_GetFrameIndex();

    return m_commandLists[idx].EmplaceBack(this);
}

GraphicsPipelineRef DebugDrawer::FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawableLayer, PassData* passData)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(passData != nullptr);

    attributes.SetDrawableLayer(drawableLayer);


    auto it = m_graphicsPipelines.Find(attributes);

    if (it != m_graphicsPipelines.End() && it->second.IsAlive())
    {
        return *it->second;
    }

    Handle<View> view = passData->view.Lock();
    Assert(view.IsValid());

    GraphicsPipelineCacheHandle cacheHandle = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        m_shader,
        m_descriptorTable,
        { &view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT), 1 },
        attributes);

    const GraphicsPipelineRef& graphicsPipeline = *cacheHandle;
    m_graphicsPipelines[attributes] = std::move(cacheHandle);

    return graphicsPipeline;
}

#pragma endregion DebugDrawer

#pragma region DebugDrawCommandList

DebugDrawCommandList::~DebugDrawCommandList()
{
    HYP_SCOPE;

    for (DebugDrawCommandHeader& header : m_headers)
    {
        if (header.destructFn)
        {
            header.destructFn(reinterpret_cast<void*>(m_buffer.Data() + header.offset));
        }
    }

    m_headers.Clear();
    m_buffer.SetSize(0);
    m_bufferOffset = 0;
}

void* DebugDrawCommandList::Alloc(uint32 size, uint32 alignment, DebugDrawCommandHeader& outHeader)
{
    HYP_SCOPE;

    AssertDebug(m_debugDrawer && m_debugDrawer->IsEnabled());
    AssertDebug(alignment <= 16);

    const uint32 alignedOffset = ByteUtil::AlignAs(m_bufferOffset, alignment);

    if (m_buffer.Size() < alignedOffset + size)
    {
        ByteBuffer newBuffer;
        newBuffer.SetSize(MathUtil::Ceil<double, SizeType>((alignedOffset + size) * 1.5));

        // move after realloc
        for (DebugDrawCommandHeader& currHeader : m_headers)
        {
            if (currHeader.moveFn)
            {
                currHeader.moveFn(reinterpret_cast<void*>(newBuffer.Data() + currHeader.offset), reinterpret_cast<void*>(m_buffer.Data() + currHeader.offset));
            }
            else
            {
                Memory::MemCpy(newBuffer.Data() + currHeader.offset, m_buffer.Data() + currHeader.offset, currHeader.size);
            }

            if (currHeader.destructFn)
            {
                currHeader.destructFn(reinterpret_cast<void*>(m_buffer.Data() + currHeader.offset));
            }
        }

        m_buffer = std::move(newBuffer);
    }

    void* ptr = m_buffer.Data() + alignedOffset;

    m_bufferOffset = alignedOffset + size;

    outHeader = {};
    outHeader.offset = alignedOffset;
    outHeader.size = size;

    return ptr;
}

void DebugDrawCommandList::Push(const DebugDrawCommandHeader& header)
{
    m_headers.PushBack(header);
}

#pragma endregion DebugDrawCommandList

} // namespace hyperion
