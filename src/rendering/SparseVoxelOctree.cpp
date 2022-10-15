#include "SparseVoxelOctree.hpp"
#include <rendering/RenderEnvironment.hpp>
#include <Engine.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::StagingBuffer;
using renderer::Result;
using ResourceState = renderer::ResourceState;
using Context = renderer::StagingBufferPool::Context;

inline static constexpr UInt32 group_x_64(UInt32 x) { return (x >> 6u) + ((x & 0x3fu) ? 1u : 0u); }

SparseVoxelOctree::SparseVoxelOctree()
    : EngineComponentBase(),
      RenderComponent(550 /* TEMP */)
{
}

SparseVoxelOctree::~SparseVoxelOctree()
{
    Teardown();
}

void SparseVoxelOctree::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    /* For now, until revoxelization is implemented */
    AssertThrow(m_octree_buffer == nullptr);

    if (m_voxelizer == nullptr) {
        m_voxelizer = std::make_unique<Voxelizer>();
        m_voxelizer->Init(engine);
    }

    CreateBuffers(engine);
    CreateDescriptors(engine);
    CreateComputePipelines(engine);
    
    SetReady(true);

    OnTeardown([this]() {
        auto *engine = GetEngine();

        SetReady(false);

        engine->GetRenderScheduler().Enqueue([this, engine](...) {
            auto result = Result::OK;

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                if (m_descriptor_sets[frame_index] == nullptr) {
                    continue;
                }

                HYPERION_PASS_ERRORS(
                    m_descriptor_sets[frame_index]->Destroy(engine->GetDevice()),
                    result
                );
            }

            if (m_counter != nullptr) {
                m_counter->Destroy(engine);
            }

            if (m_build_info_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_build_info_buffer->Destroy(engine->GetDevice()),
                    result
                );
            }

            if (m_indirect_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_indirect_buffer->Destroy(engine->GetDevice()),
                    result
                );
            }

            if (m_octree_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_octree_buffer->Destroy(engine->GetDevice()),
                    result
                );
            }

            return result;
        });

        HYP_FLUSH_RENDER_QUEUE(engine);

        m_voxelizer.reset(nullptr);

        m_counter.reset(nullptr);

        m_build_info_buffer.reset(nullptr);
        m_indirect_buffer.reset(nullptr);
        m_octree_buffer.reset(nullptr);
    
        m_alloc_nodes.Reset();
        m_init_nodes.Reset();
        m_tag_nodes.Reset();
        m_modify_args.Reset();
        m_write_mipmaps.Reset();
    });
}

// called from game thread
void SparseVoxelOctree::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);

    for (auto &it : GetParent()->GetScene()->GetEntities()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        const auto &renderable_attributes = entity->GetRenderableAttributes();

        if (BucketHasGlobalIllumination(renderable_attributes.material_attributes.bucket)
            && (renderable_attributes.mesh_attributes.vertex_attributes
                & m_voxelizer->GetRendererInstance()->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {

            m_voxelizer->GetRendererInstance()->AddEntity(Handle<Entity>(it.second));
        }
    }
}

void SparseVoxelOctree::OnEntityAdded(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    const auto &renderable_attributes = entity->GetRenderableAttributes();

    if (BucketHasGlobalIllumination(renderable_attributes.material_attributes.bucket)
        && (renderable_attributes.mesh_attributes.vertex_attributes
            & m_voxelizer->GetRendererInstance()->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {
        m_voxelizer->GetRendererInstance()->AddEntity(Handle<Entity>(entity));
    }
}

void SparseVoxelOctree::OnEntityRemoved(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_voxelizer->GetRendererInstance()->RemoveEntity(Handle<Entity>(entity));
}

void SparseVoxelOctree::OnEntityRenderableAttributesChanged(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    const auto &renderable_attributes = entity->GetRenderableAttributes();

    if (BucketHasGlobalIllumination(renderable_attributes.material_attributes.bucket)
        && (renderable_attributes.mesh_attributes.vertex_attributes
            & m_voxelizer->GetRendererInstance()->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {
        m_voxelizer->GetRendererInstance()->AddEntity(Handle<Entity>(entity));
    } else {
        m_voxelizer->GetRendererInstance()->RemoveEntity(Handle<Entity>(entity));
    }
}

void SparseVoxelOctree::OnUpdate(Engine *engine, GameCounter::TickUnit delta)
{
    // Threads::AssertOnThread(THREAD_GAME);
    AssertReady();
}

void SparseVoxelOctree::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

UInt32 SparseVoxelOctree::CalculateNumNodes() const
{
    AssertThrow(m_voxelizer != nullptr);

    const UInt32 fragment_count = m_voxelizer->NumFragments();

    UInt32 num_nodes = MathUtil::Max(fragment_count << 3u, min_nodes);

    if (num_nodes > max_nodes) {
        DebugLog(
            LogType::Warn,
            "Calculated as requiring %llu nodes which is greater than maximum of %llu, capping at max\n",
            num_nodes,
            max_nodes
        );

        num_nodes = max_nodes;
    }

    return num_nodes;
}

void SparseVoxelOctree::CreateBuffers(Engine *engine)
{
    m_build_info_buffer = std::make_unique<StorageBuffer>();
    m_indirect_buffer = std::make_unique<IndirectBuffer>();
    m_counter = std::make_unique<AtomicCounter>();

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        m_counter->Create(engine);

        HYPERION_PASS_ERRORS(
            m_build_info_buffer->Create(engine->GetInstance()->GetDevice(), 2 * sizeof(UInt32)),
            result
        );

        HYPERION_PASS_ERRORS(
            m_indirect_buffer->Create(engine->GetInstance()->GetDevice(), 3 * sizeof(UInt32)),
            result
        );

        m_octree_buffer = std::make_unique<StorageBuffer>();

        const auto num_nodes = CalculateNumNodes();

        DebugLog(
            LogType::Debug,
            "%llu rendered fragments, creating %llu octree nodes (%llu MiB)\n",
            m_voxelizer->NumFragments(),
            num_nodes,
            (num_nodes * sizeof(OctreeNode)) / 1000000
        );

        HYPERION_PASS_ERRORS(
            m_octree_buffer->Create(engine->GetInstance()->GetDevice(), num_nodes * sizeof(OctreeNode)),
            result
        );

        if (!result) {
            engine->SafeRelease(UniquePtr<renderer::StorageBuffer>(m_octree_buffer.release()));

            HYPERION_PASS_ERRORS(
                m_build_info_buffer->Destroy(engine->GetInstance()->GetDevice()),
                result
            );

            m_build_info_buffer.reset(nullptr);

            HYPERION_PASS_ERRORS(
                m_indirect_buffer->Destroy(engine->GetInstance()->GetDevice()),
                result
            );

            m_indirect_buffer.reset(nullptr);
        }

        return result;
    });
}

void SparseVoxelOctree::CreateDescriptors(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_descriptor_sets[frame_index] = UniquePtr<DescriptorSet>::Construct();
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto &descriptor_set = m_descriptor_sets[frame_index];
            AssertThrow(descriptor_set != nullptr);

            /* 0 = voxel atomic counter */
            /* 1 = fragment list  */
            /* 2 = octree         */
            /* 3 = build info     */
            /* 4 = indirect       */
            /* 5 = octree atomic counter */
    
            descriptor_set
                ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
                ->SetSubDescriptor({ .buffer = m_voxelizer->GetAtomicCounter()->GetBuffer() });
    
            descriptor_set
                ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
                ->SetSubDescriptor({ .buffer = m_voxelizer->GetFragmentListBuffer() });

            descriptor_set
                ->AddDescriptor<renderer::StorageBufferDescriptor>(2)
                ->SetSubDescriptor({ .buffer = m_octree_buffer.get() });

            descriptor_set
                ->AddDescriptor<renderer::StorageBufferDescriptor>(3)
                ->SetSubDescriptor({ .buffer = m_build_info_buffer.get() });

            descriptor_set
                ->AddDescriptor<renderer::StorageBufferDescriptor>(4)
                ->SetSubDescriptor({ .buffer = m_indirect_buffer.get() });

            descriptor_set
                ->AddDescriptor<renderer::StorageBufferDescriptor>(5)
                ->SetSubDescriptor({ .buffer = m_counter->GetBuffer() });

            HYPERION_BUBBLE_ERRORS(descriptor_set->Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));

            // set octree buffer in global descriptor set
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
            
            descriptor_set_globals->GetDescriptor(DescriptorKey::SVO_BUFFER)->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_octree_buffer.get()
            });
        }

        HYPERION_RETURN_OK;
    });
}

void SparseVoxelOctree::CreateComputePipelines(Engine *engine)
{
    m_alloc_nodes = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_alloc_nodes.comp.spv")).Read()}}
            }
        ),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(engine->InitObject(m_alloc_nodes));

    m_init_nodes = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_init_nodes.comp.spv")).Read()}}
            }
        ),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(engine->InitObject(m_init_nodes));

    m_tag_nodes = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_tag_nodes.comp.spv")).Read()}}
            }
        ),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(engine->InitObject(m_tag_nodes));

    m_modify_args = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_modify_args.comp.spv")).Read()}}
            }
        ),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(engine->InitObject(m_modify_args));

    m_write_mipmaps = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_write_mipmaps.comp.spv")).Read()}}
            }
        ),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(engine->InitObject(m_write_mipmaps));
}

void SparseVoxelOctree::OnRender(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(m_voxelizer != nullptr);
    m_voxelizer->Render(engine, frame);
    
    // temp
    m_descriptor_sets[0]
        ->GetDescriptor(1)
        ->SetSubDescriptor({ .element_index = 0u, .buffer = m_voxelizer->GetFragmentListBuffer() });

    m_descriptor_sets[0]->ApplyUpdates(engine->GetDevice());

    m_counter->Reset(engine);

    static constexpr UInt32 build_info[] = { 0, 8 };
    static constexpr UInt32 indirect_info[] = { 1, 1, 1 };

    const renderer::ComputePipeline::PushConstantData push_constants {
        .octree_data = {
            .num_fragments = m_voxelizer->NumFragments(),
            .voxel_grid_size = static_cast<UInt32>(m_voxelizer->voxel_map_size),
            .mipmap_level = 0u
        }
    };

    /* resize node buffer */
    const UInt32 num_nodes = CalculateNumNodes();

    if (num_nodes * sizeof(OctreeNode) > m_octree_buffer->size) {
        DebugLog(
            LogType::Debug,
            "Resizing octree buffer to %llu nodes (%llu MiB)\n",
            num_nodes,
            (num_nodes * sizeof(OctreeNode)) / 1000000
        );

        engine->SafeRelease(UniquePtr<renderer::StorageBuffer>(m_octree_buffer.release()));

        // HYPERION_ASSERT_RESULT(m_octree_buffer->Destroy(engine->GetDevice()));
        m_octree_buffer.reset(new StorageBuffer());
        HYPERION_ASSERT_RESULT(m_octree_buffer->Create(engine->GetDevice(), num_nodes * sizeof(OctreeNode)));

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
            
            descriptor_set_globals->GetDescriptor(DescriptorKey::SVO_BUFFER)->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_octree_buffer.get()
            });

            // apply to own descriptor sets too
            m_descriptor_sets[frame_index]->GetDescriptor(2)->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_octree_buffer.get()
            });

            m_descriptor_sets[frame_index]->ApplyUpdates(engine->GetDevice());
        }
    }
    
    HYPERION_ASSERT_RESULT(engine->GetInstance()->GetStagingBufferPool().Use(
        engine->GetInstance()->GetDevice(),
        [&](Context &context) {
            auto *device = engine->GetDevice();

            StagingBuffer *build_info_staging_buffer = context.Acquire(m_build_info_buffer->size);
            build_info_staging_buffer->Copy(device, std::size(build_info) * sizeof(UInt32), build_info);

            StagingBuffer *indirect_staging_buffer = context.Acquire(m_indirect_buffer->size);
            indirect_staging_buffer->Copy(device, std::size(indirect_info) * sizeof(UInt32), indirect_info);

            auto commands = engine->GetInstance()->GetSingleTimeCommands();

            /* Copy our data from staging buffers */
            commands.Push([&](CommandBuffer *command_buffer) {
                m_build_info_buffer->CopyFrom(command_buffer, build_info_staging_buffer, std::size(build_info) * sizeof(UInt32));
                m_build_info_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                m_indirect_buffer->CopyFrom(command_buffer, indirect_staging_buffer, std::size(indirect_info) * sizeof(UInt32));
                m_indirect_buffer->InsertBarrier(command_buffer, ResourceState::INDIRECT_ARG);

                HYPERION_RETURN_OK;
            });

	        UInt32 fragment_group_x = group_x_64(m_voxelizer->NumFragments());

            for (UInt i = 1; i <= m_voxelizer->octree_depth; i++) {
                commands.Push([&, index = i](CommandBuffer *command_buffer) {
                    m_init_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSets(engine, command_buffer, 0, m_init_nodes.Get());
                    m_init_nodes->GetPipeline()->DispatchIndirect(command_buffer, m_indirect_buffer.get());

                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    m_tag_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSets(engine, command_buffer, 0, m_tag_nodes.Get());
                    m_tag_nodes->GetPipeline()->Dispatch(command_buffer, {fragment_group_x, 1, 1});

                    if (index == m_voxelizer->octree_depth) {
                        HYPERION_RETURN_OK;
                    }
                    
                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);
                    
                    m_alloc_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSets(engine, command_buffer, 0, m_alloc_nodes.Get());
                    m_alloc_nodes->GetPipeline()->DispatchIndirect(command_buffer, m_indirect_buffer.get());

                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    m_modify_args->GetPipeline()->Bind(command_buffer);
                    BindDescriptorSets(engine, command_buffer, 0, m_modify_args.Get());
                    m_modify_args->GetPipeline()->Dispatch(command_buffer, {1, 1, 1});
                    
                    m_indirect_buffer->InsertBarrier(command_buffer, ResourceState::INDIRECT_ARG);
                    m_build_info_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    HYPERION_RETURN_OK;
                });
            }

            return commands.Execute(device);
        }
    ));

    WriteMipmaps(engine);
}

void SparseVoxelOctree::WriteMipmaps(Engine *engine)
{
    renderer::ComputePipeline::PushConstantData push_constants {
        .octree_data = {
            .num_fragments = m_voxelizer->NumFragments(),
            .voxel_grid_size = static_cast<UInt32>(m_voxelizer->voxel_map_size)
        }
    };

	const UInt32 fragment_group_x = group_x_64(m_voxelizer->NumFragments());

    auto commands = engine->GetInstance()->GetSingleTimeCommands();

    commands.Push([&](CommandBuffer *command_buffer) {
        for (UInt32 i = 2; i <= m_voxelizer->octree_depth; i++) {
            push_constants.octree_data.mipmap_level = i;

            m_write_mipmaps->GetPipeline()->Bind(command_buffer, push_constants);
            BindDescriptorSets(engine, command_buffer, 0, m_write_mipmaps.Get());
            m_write_mipmaps->GetPipeline()->Dispatch(command_buffer, {fragment_group_x, 1, 1});

            if (i != m_voxelizer->octree_depth) {
                m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);
            }
        }

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(commands.Execute(engine->GetDevice()));
}

void SparseVoxelOctree::BindDescriptorSets(
    Engine *engine,
    CommandBuffer *command_buffer,
    UInt frame_index,
    const ComputePipeline *pipeline
) const
{
    command_buffer->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        pipeline->GetPipeline(),
        m_descriptor_sets[frame_index].Get(),
        static_cast<DescriptorSet::Index>(0)
    );
}

} // namespace hyperion::v2
