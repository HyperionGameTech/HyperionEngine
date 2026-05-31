/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Rendering/DebugDrawer.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/RawBufferAllocator.hpp>
#include <Rendering/CBufferAllocator.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/EngineDriver.hpp>

#include <Scene/View.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/EnvGrid.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Core/Memory/Resource/Resource.hpp>

#include <DebugDrawer.generated.inl>

namespace Hyperion {

extern EngineStatCounter<uint32> g_statDebugDraws;

static const ShaderPropertyId s_propImmediateMode = InternShaderProperty(ShaderProperty(NAME("IMMEDIATE_MODE")));

static RenderableAttributeSet GetRenderableAttributes()
{
    return RenderableAttributeSet(
        MeshAttributes {
            .inputLayout = StaticVertexInputLayout<VT_Simple>,
            .topology = TOP_LINES
        },
        MaterialAttributes {
            .bucket = RenderBucket::Debug,
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction::None(),
            .flags = MAF_DEPTH_TEST | MAF_DEPTH_WRITE
        });
}

struct DebugDrawCommand
{
    IDebugDrawShape* shape;
    Mat4f transformMatrix;
    Color color;
    RenderableAttributeSet attributes;
};

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
static int s_debugDrawShapeIdCounter = 0;

static inline int NextShapeId()
{
    int shapeId = s_debugDrawShapeIdCounter++;
    AssertDebug(shapeId < MaxDebugDrawShapeTypes);

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
        DelegateHandler onShutdownHandle;

        MeshInitializer()
        {
            mesh = MeshBuilder::NormalizedCubeSphere(4);
            mesh->SetFlags(MeshFlags::ViewIndependent);
            mesh->SetName(NAME("SphereDebugDrawShape"));
            mesh->UploadGpuData();

            GetEngineAssetRegistry()->PutAsset(mesh);

            onShutdownHandle = g_engineDriver->GetDelegates().OnShutdown.Bind([m = &mesh]()
            {
                m->Reset();
            });
        }
    } s_initializer;

    return s_initializer.mesh;
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
        Transform(position, radius, Quat4f::Identity()).GetMatrix(),
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

    const uint32 envProbeIndex = Resources::GetBinding(static_cast<DebugDrawCommand_Probe*>(cmd)->envProbe);

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
    ptr->transformMatrix = Transform(position, radius, Quat4f::Identity()).GetMatrix();
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

    const uint32 envProbeIndex = Resources::GetBinding(static_cast<DebugDrawCommand_Probe*>(cmd)->envProbe);

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
    ptr->transformMatrix = Transform(position, radius, Quat4f::Identity()).GetMatrix();
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
        DelegateHandler onShutdownHandle;

        MeshInitializer()
        {
            mesh = MeshBuilder::Cube();
            mesh->SetFlags(MeshFlags::ViewIndependent);
            mesh->SetName(NAME("BoxDebugDrawShape"));
            mesh->UploadGpuData();

            GetEngineAssetRegistry()->PutAsset(mesh);

            onShutdownHandle = g_engineDriver->GetDelegates().OnShutdown.Bind([m = &mesh]()
            {
                m->Reset();
            });
        }
    } s_initializer;

    return s_initializer.mesh;
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
        Transform(position, size, Quat4f::Identity()).GetMatrix(),
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
        DelegateHandler onShutdownHandle;

        MeshInitializer()
        {
            mesh = MeshBuilder::Quad();
            mesh->SetFlags(MeshFlags::ViewIndependent);
            mesh->SetName(NAME("PlaneDebugDrawShape"));
            mesh->UploadGpuData();

            GetEngineAssetRegistry()->PutAsset(mesh);

            onShutdownHandle = g_engineDriver->GetDelegates().OnShutdown.Bind([m = &mesh]()
            {
                m->Reset();
            });
        }
    } s_initializer;

    return s_initializer.mesh;
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

    Mat4f transformMatrix;

    transformMatrix.rows[0][0] = x.x;
    transformMatrix.rows[0][1] = x.y;
    transformMatrix.rows[0][2] = x.z;

    transformMatrix.rows[1][0] = y.x;
    transformMatrix.rows[1][1] = y.y;
    transformMatrix.rows[1][2] = y.z;

    transformMatrix.rows[2][0] = z.x;
    transformMatrix.rows[2][1] = z.y;
    transformMatrix.rows[2][2] = z.z;

    transformMatrix.rows[3][0] = center.x;
    transformMatrix.rows[3][1] = center.y;
    transformMatrix.rows[3][2] = center.z;
    transformMatrix.rows[3][3] = 1.0f;

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

static FixedArray<ByteBuffer, RingBufferDepth> CreateDebugDrawBuffers()
{
    ValueStorage<FixedArray<ByteBuffer, RingBufferDepth>> buffersStorage;

    for (uint32 i = 0; i < RingBufferDepth; i++)
    {
        new (buffersStorage.GetPointer()->Data() + i) ByteBuffer;
    }

    return std::move(buffersStorage).Get();
}

DebugDrawer& DebugDrawer::GetInstance()
{
    static DebugDrawer s_instance;
    return s_instance;
}

DebugDrawer::DebugDrawer()
    : m_config(DebugDrawerConfig::FromConfig()),
      m_buffers(CreateDebugDrawBuffers()),
      m_bufferOffsets {},
      m_bufferSizeHistory {}
{
}

DebugDrawer::~DebugDrawer()
{
}

void DebugDrawer::Initialize()
{
}

void DebugDrawer::Shutdown()
{
    for (uint32 i = 0; i < uint32(m_commandLists.Size()); i++)
    {
        m_commandLists[i].Clear();
    }

    for (Array<ImmediateDrawShaderData, RenderAllocator>& data : m_cachedPartitionedShaderData)
    {
        data.Clear();
        data.Refit();
    }

    for (uint32 i = 0; i < uint32(m_buffers.Size()); i++)
    {
        if (m_buffers[i].GetCapacity() == 0)
        {
            continue;
        }

        struct DebugDrawBufferDeleterPayload
        {
            Array<DebugDrawCommandHeader> headers;
            ByteBuffer buffer;
        };

        struct DebugDrawBufferDeleter
        {
            uint32 idx;
            DebugDrawBufferDeleterPayload* payload;
        };

        Mutex::Guard* pGuard = nullptr;
        HYP_DEFER({ if (pGuard) delete pGuard; });

        // safely destroy the buffer data on the correct frame
        DebugDrawBufferDeleter* deleter = DeletionQueue::GetInstance().AllocCustom<DebugDrawBufferDeleter>([](void* ptr)
            {
                AssertOnThread(g_renderThread);

                DebugDrawBufferDeleter* del = reinterpret_cast<DebugDrawBufferDeleter*>(ptr);
                AssertDebug(del->idx == GetRingIndex());

                DebugDrawBufferDeleterPayload* payload = del->payload;
                AssertDebug(payload != nullptr);

                // invoke destructors
                for (DebugDrawCommandHeader& header : payload->headers)
                {
                    if (header.destructFn)
                    {
                        header.destructFn(reinterpret_cast<void*>(payload->buffer.Data() + header.offset));
                    }
                }

                delete del->payload;
            },
            &pGuard,
            /* desiredIdx */ i);

        deleter->idx = i;
        deleter->payload = new DebugDrawBufferDeleterPayload {
            .headers = std::move(m_headers[i]),
            .buffer = std::move(m_buffers[i])
        };
    }
}

void DebugDrawer::Update()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const uint32 idx = GetRingIndex();

    if (m_commandLists[idx].Empty())
    {
        return;
    }

    auto& buffer = m_buffers[idx];
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
                newBuffer.SetSize(MathUtil::Ceil<double, size_t>((newAlignedOffset + header.size) * 1.5));

                // have to move all current commands since the buffer will realloc
                for (DebugDrawCommandHeader& currHeader : m_headers[idx])
                {
                    if (currHeader.moveFn)
                    {
                        currHeader.moveFn(reinterpret_cast<void*>(newBuffer.Data() + currHeader.offset), reinterpret_cast<void*>(buffer.Data() + currHeader.offset));
                    }
                    else
                    {
                        Memory::Copy(newBuffer.Data() + currHeader.offset, buffer.Data() + currHeader.offset, currHeader.size);
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
                Memory::Copy(buffer.Data() + newAlignedOffset, vp, header.size);
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
        it.m_buffer.Clear();
        it.m_bufferOffset = 0;
    }
}

void DebugDrawer::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 idx = GetRingIndex();

    if (!IsEnabled() || m_headers[idx].Empty())
    {
        // clear, otherwise we'll start to leak a huge amount of memory
        ClearCommands(idx);

        return;
    }

    Assert(renderSetup.HasView());

    const Viewport& viewport = renderSetup.viewport;

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    CommandRecorder& cr = frame->cr;

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
    IDebugDrawShape* currShapes[MaxDebugDrawShapeTypes] = { };

    for (size_t drawCommandIdx = 0; drawCommandIdx < m_headers[idx].Size(); drawCommandIdx++)
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

        AssertDebug(shapeIdx >= 0 && shapeIdx < MaxDebugDrawShapeTypes);

        auto& shaderData = partitionedShaderData[shapeIdx];
        currShapes[shapeIdx] = drawCommand->shape;

        ImmediateDrawShaderData& shaderDataElement = shaderData.EmplaceBack();

        drawCommand->shape->UpdateBufferData(drawCommand, &shaderDataElement);

        shaderDataElement.idx = (uint32)drawCommandIdx;
    }

    RenderableAttributeSet attributes;

    uint32 elemOffset = 0;
    uint32 totalDrawCalls = 0;
    uint32 totalInstancedDraws = 0;

    ShaderDesc shaderDesc;
    shaderDesc.name = NAME("DebugVis");
    shaderDesc.properties.Add(s_propImmediateMode);

    cr << SetCurrentShader(shaderDesc);
    cr << SetCurrentViewport(viewport);

    HYP_DEFER({
        // reset states
        cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
        cr << SetTopology(TOP_TRIANGLES);
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
        cr << SetStencilTest(false);
        cr << SetFillMode(FM_FILL);
        cr << SetFaceCullMode(FCM_BACK);
    });

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    StructuredBuffer& instanceBuffer = RI.bufferAllocator->AcquireStructuredBuffer(m_headers[idx].Size(), sizeof(ImmediateDrawShaderData));

    cr << SetShaderUniform(0, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(1, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(2, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));
    cr << SetShaderUniform(3, "EnvProbesTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesTexture));

    cr << SetShaderUniform(4, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(5, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(6, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    cr << SetShaderUniform(7, "ImmediateDrawsBuffer"_sh, instanceBuffer);

    for (size_t shapeIdx = 0; shapeIdx < std::size(partitionedShaderData); shapeIdx++)
    {
        auto& shaderData = partitionedShaderData[shapeIdx];

        if (shaderData.Empty())
        {
            continue;
        }

        instanceBuffer.Write(elemOffset * sizeof(ImmediateDrawShaderData), shaderData.Size() * sizeof(ImmediateDrawShaderData), shaderData.Data());

        uint32 numToDraw = 0;

        auto CommitCurrentDraws = [&]()
        {
            if (numToDraw != 0)
            {
                g_statDebugDraws += numToDraw;

                IDebugDrawShape* shape = currShapes[shapeIdx];
                AssertDebug(shape != nullptr);

                switch (shape->GetDebugDrawType())
                {
                case DebugDrawType::MESH:
                {
                    cr << SetTopology(attributes.GetMeshAttributes().topology);
                    cr << SetInputLayout(attributes.GetMeshAttributes().inputLayout);

                    cr << SetCurrentBlendFunction(attributes.GetMaterialAttributes().blendFunction);
                    cr << SetFaceCullMode(attributes.GetMaterialAttributes().cullFaces);
                    cr << SetFillMode(attributes.GetMaterialAttributes().fillMode);
                    cr << SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
                    cr << SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
                    cr << SetStencilTest(bool(attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST));
                    cr << SetStencilFunction(attributes.GetMaterialAttributes().stencilFunction);

                    GpuBuffer* cbuffer = nullptr;
                    size_t cbufferOffset = 0;
                    size_t cbufferSize = 0;

                    // Write immediate draw base offset
                    RI.cbufferAllocator->Write<uint32>(&elemOffset);
                    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

                    cr << SetShaderUniform(8, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

                    cr << CommitDrawState();

                    MeshDebugDrawShapeBase* meshShape = static_cast<MeshDebugDrawShapeBase*>(shape);

                    Mesh* mesh = meshShape->GetMesh();
                    AssertDebug(mesh != nullptr);

                    cr << BindVertexBuffer(mesh->GetVertexBuffer());
                    cr << BindIndexBuffer(mesh->GetIndexBuffer());
                    cr << DrawIndexed(mesh->NumIndices(), numToDraw);

                    ++totalDrawCalls;
                    totalInstancedDraws += numToDraw;

                    break;
                }
                default:
                    HYP_NOT_IMPLEMENTED();
                }

                elemOffset += numToDraw;
                numToDraw = 0;
            }
        };

        for (size_t i = 0; i < shaderData.Size(); i++)
        {
            AssertDebug(shaderData[i].idx != ~0u, "culled element should not be in array");

            const uint32 drawCommandIdx = shaderData[i].idx;

            uint32 offset = m_headers[idx][drawCommandIdx].offset;
            uint32 size = m_headers[idx][drawCommandIdx].size;
            AssertDebug(offset + size <= m_buffers[idx].Size());

            numToDraw++;

            DebugDrawCommand* drawCommand = reinterpret_cast<DebugDrawCommand*>(m_buffers[idx].Data() + offset);

            if (attributes != drawCommand->attributes)
            {
                attributes = drawCommand->attributes;

                if (i != 0)
                {
                    CommitCurrentDraws();
                }
            }
        }

        if (numToDraw != 0)
        {
            CommitCurrentDraws();
        }
    }

    instanceBuffer.Flush();

    ClearCommands(idx);
}

void DebugDrawer::ClearCommands(uint32 idx)
{
    HYP_SCOPE;
    AssertDebug(idx < m_commandLists.Size());

    // would cause issues if we try to free from pool being used by wrong thread..
    Assert(idx == GetRingIndex());

    for (DebugDrawCommandHeader& header : m_headers[idx])
    {
        if (header.destructFn)
        {
            header.destructFn(reinterpret_cast<void*>(m_buffers[idx].Data() + header.offset));
        }
    }

    // update buffer capacity based on history so we don't keep memory around longer than we need to.
    size_t maxHistorySize = 0;

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
    AssertOnThread(g_simThread | g_renderThread);

    const uint32 idx = GetRingIndex();

    return m_commandLists[idx].EmplaceBack(this);
}

#pragma endregion DebugDrawer

#pragma region DebugDrawCommandList

DebugDrawCommandList::DebugDrawCommandList(DebugDrawer* debugDrawer)
    : m_debugDrawer(debugDrawer),
      m_lock(debugDrawer->m_sharedMutex),
      sphere(*this),
      ambientProbe(*this),
      reflectionProbe(*this),
      box(*this),
      plane(*this),
      m_bufferOffset(0)
{
}

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
        newBuffer.SetSize(MathUtil::Ceil<double, size_t>((alignedOffset + size) * 1.5));

        // move after realloc
        for (DebugDrawCommandHeader& currHeader : m_headers)
        {
            if (currHeader.moveFn)
            {
                currHeader.moveFn(reinterpret_cast<void*>(newBuffer.Data() + currHeader.offset), reinterpret_cast<void*>(m_buffer.Data() + currHeader.offset));
            }
            else
            {
                Memory::Copy(newBuffer.Data() + currHeader.offset, m_buffer.Data() + currHeader.offset, currHeader.size);
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

} // namespace Hyperion
