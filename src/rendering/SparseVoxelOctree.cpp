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

void SparseVoxelOctree::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init;

    /* For now, until revoxelization is implemented */
    AssertThrow(m_octree_buffer == nullptr);

    if (m_voxelizer == nullptr) {
        m_voxelizer = std::make_unique<Voxelizer>();
        m_voxelizer->Init;
    }

    CreateBuffers;
    CreateDescriptors;
    CreateComputePipelines;
    
    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        struct RENDER_COMMAND(DestroySVO) : RenderCommandBase2
        {
            SparseVoxelOctree &svo;

            RENDER_COMMAND(DestroySVO)(SparseVoxelOctree &svo)
                : svo(svo)
            {
            }

            virtual Result operator()()
            {
                auto result = Result::OK;

                for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                    if (svo.m_descriptor_sets[frame_index] == nullptr) {
                        continue;
                    }

                    HYPERION_PASS_ERRORS(
                        svo.m_descriptor_sets[frame_index]->Destroy(Engine::Get()->GetDevice()),
                        result
                    );
                }

                if (svo.m_counter != nullptr) {
                    svo.m_counter->Destroy;
                }

                if (svo.m_build_info_buffer != nullptr) {
                    HYPERION_PASS_ERRORS(
                        svo.m_build_info_buffer->Destroy(Engine::Get()->GetDevice()),
                        result
                    );
                }

                if (svo.m_indirect_buffer != nullptr) {
                    HYPERION_PASS_ERRORS(
                        svo.m_indirect_buffer->Destroy(Engine::Get()->GetDevice()),
                        result
                    );
                }

                if (svo.m_octree_buffer != nullptr) {
                    HYPERION_PASS_ERRORS(
                        svo.m_octree_buffer->Destroy(Engine::Get()->GetDevice()),
                        result
                    );
                }

                return result;
            }
        };

        RenderCommands::Push<RENDER_COMMAND(DestroySVO)>(*this);

        HYP_FLUSH_RENDER_QUEUE();

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
void SparseVoxelOctree::InitGame()
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

void SparseVoxelOctree::OnUpdate( GameCounter::TickUnit delta)
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

void SparseVoxelOctree::CreateBuffers()
{
    m_build_info_buffer = std::make_unique<StorageBuffer>();
    m_indirect_buffer = std::make_unique<IndirectBuffer>();
    m_counter = std::make_unique<AtomicCounter>();

    struct RENDER_COMMAND(CreateSVOBuffers) : RenderCommandBase2
    {
        SparseVoxelOctree &svo;

        RENDER_COMMAND(CreateSVOBuffers)(SparseVoxelOctree &svo)
            : svo(svo)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            svo.m_counter->Create;

            HYPERION_PASS_ERRORS(
                svo.m_build_info_buffer->Create(Engine::Get()->GetInstance()->GetDevice(), 2 * sizeof(UInt32)),
                result
            );

            HYPERION_PASS_ERRORS(
                svo.m_indirect_buffer->Create(Engine::Get()->GetInstance()->GetDevice(), 3 * sizeof(UInt32)),
                result
            );

            svo.m_octree_buffer = std::make_unique<StorageBuffer>();

            const auto num_nodes = svo.CalculateNumNodes();

            DebugLog(
                LogType::Debug,
                "%llu rendered fragments, creating %llu octree nodes (%llu MiB)\n",
                svo.m_voxelizer->NumFragments(),
                num_nodes,
                (num_nodes * sizeof(OctreeNode)) / 1000000
            );

            HYPERION_PASS_ERRORS(
                svo.m_octree_buffer->Create(Engine::Get()->GetInstance()->GetDevice(), num_nodes * sizeof(OctreeNode)),
                result
            );

            if (!result) {
                Engine::Get()->SafeRelease(UniquePtr<renderer::StorageBuffer>(svo.m_octree_buffer.release()));

                HYPERION_PASS_ERRORS(
                    svo.m_build_info_buffer->Destroy(Engine::Get()->GetInstance()->GetDevice()),
                    result
                );

                svo.m_build_info_buffer.reset(nullptr);

                HYPERION_PASS_ERRORS(
                    svo.m_indirect_buffer->Destroy(Engine::Get()->GetInstance()->GetDevice()),
                    result
                );

                svo.m_indirect_buffer.reset(nullptr);
            }

            return result;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateSVOBuffers)>(*this);
}

void SparseVoxelOctree::CreateDescriptors()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_descriptor_sets[frame_index] = UniquePtr<DescriptorSet>::Construct();
    }

    struct RENDER_COMMAND(CreateSVODescriptors) : RenderCommandBase2
    {
        SparseVoxelOctree &svo;

        RENDER_COMMAND(CreateSVODescriptors)(SparseVoxelOctree &svo)
            : svo(svo)
        {
        }

        virtual Result operator()()
        {
            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                auto &descriptor_set = svo.m_descriptor_sets[frame_index];
                AssertThrow(descriptor_set != nullptr);

                /* 0 = voxel atomic counter */
                /* 1 = fragment list  */
                /* 2 = octree         */
                /* 3 = build info     */
                /* 4 = indirect       */
                /* 5 = octree atomic counter */
        
                descriptor_set
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
                    ->SetSubDescriptor({ .buffer = svo.m_voxelizer->GetAtomicCounter()->GetBuffer() });
        
                descriptor_set
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
                    ->SetSubDescriptor({ .buffer = svo.m_voxelizer->GetFragmentListBuffer() });

                descriptor_set
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(2)
                    ->SetSubDescriptor({ .buffer = svo.m_octree_buffer.get() });

                descriptor_set
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(3)
                    ->SetSubDescriptor({ .buffer = svo.m_build_info_buffer.get() });

                descriptor_set
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(4)
                    ->SetSubDescriptor({ .buffer = svo.m_indirect_buffer.get() });

                descriptor_set
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(5)
                    ->SetSubDescriptor({ .buffer = svo.m_counter->GetBuffer() });

                HYPERION_BUBBLE_ERRORS(descriptor_set->Create(
                    Engine::Get()->GetDevice(),
                    &Engine::Get()->GetInstance()->GetDescriptorPool()
                ));

                // set octree buffer in global descriptor set
                auto *descriptor_set_globals = Engine::Get()->GetInstance()->GetDescriptorPool()
                    .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
                
                descriptor_set_globals->GetDescriptor(DescriptorKey::SVO_BUFFER)->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = svo.m_octree_buffer.get()
                });
            }

            HYPERION_RETURN_OK;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateSVODescriptors)>(*this);
}

void SparseVoxelOctree::CreateComputePipelines()
{
    m_alloc_nodes = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_alloc_nodes.comp.spv")).Read()}}
            }
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(Engine::Get()->InitObject(m_alloc_nodes));

    m_init_nodes = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_init_nodes.comp.spv")).Read()}}
            }
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(Engine::Get()->InitObject(m_init_nodes));

    m_tag_nodes = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_tag_nodes.comp.spv")).Read()}}
            }
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(Engine::Get()->InitObject(m_tag_nodes));

    m_modify_args = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_modify_args.comp.spv")).Read()}}
            }
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(Engine::Get()->InitObject(m_modify_args));

    m_write_mipmaps = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "vkshaders/voxel/octree_write_mipmaps.comp.spv")).Read()}}
            }
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    AssertThrow(Engine::Get()->InitObject(m_write_mipmaps));
}

void SparseVoxelOctree::OnRender( Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(m_voxelizer != nullptr);
    m_voxelizer->Renderframe);
    
    // temp
    m_descriptor_sets[0]
        ->GetDescriptor(1)
        ->SetSubDescriptor({ .element_index = 0u, .buffer = m_voxelizer->GetFragmentListBuffer() });

    m_descriptor_sets[0]->ApplyUpdates(Engine::Get()->GetDevice());

    m_counter->Reset;

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

        Engine::Get()->SafeRelease(UniquePtr<renderer::StorageBuffer>(m_octree_buffer.release()));

        // HYPERION_ASSERT_RESULT(m_octree_buffer->Destroy(Engine::Get()->GetDevice()));
        m_octree_buffer.reset(new StorageBuffer());
        HYPERION_ASSERT_RESULT(m_octree_buffer->Create(Engine::Get()->GetDevice(), num_nodes * sizeof(OctreeNode)));

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set_globals = Engine::Get()->GetInstance()->GetDescriptorPool()
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

            m_descriptor_sets[frame_index]->ApplyUpdates(Engine::Get()->GetDevice());
        }
    }
    
    HYPERION_ASSERT_RESULT(Engine::Get()->GetInstance()->GetStagingBufferPool().Use(
        Engine::Get()->GetInstance()->GetDevice(),
        [&](Context &context) {
            auto *device = Engine::Get()->GetDevice();

            StagingBuffer *build_info_staging_buffer = context.Acquire(m_build_info_buffer->size);
            build_info_staging_buffer->Copy(device, std::size(build_info) * sizeof(UInt32), build_info);

            StagingBuffer *indirect_staging_buffer = context.Acquire(m_indirect_buffer->size);
            indirect_staging_buffer->Copy(device, std::size(indirect_info) * sizeof(UInt32), indirect_info);

            auto commands = Engine::Get()->GetInstance()->GetSingleTimeCommands();

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
                    BindDescriptorSetscommand_buffer, 0, m_init_nodes.Get());
                    m_init_nodes->GetPipeline()->DispatchIndirect(command_buffer, m_indirect_buffer.get());

                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    m_tag_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSetscommand_buffer, 0, m_tag_nodes.Get());
                    m_tag_nodes->GetPipeline()->Dispatch(command_buffer, {fragment_group_x, 1, 1});

                    if (index == m_voxelizer->octree_depth) {
                        HYPERION_RETURN_OK;
                    }
                    
                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);
                    
                    m_alloc_nodes->GetPipeline()->Bind(command_buffer, push_constants);
                    BindDescriptorSetscommand_buffer, 0, m_alloc_nodes.Get());
                    m_alloc_nodes->GetPipeline()->DispatchIndirect(command_buffer, m_indirect_buffer.get());

                    m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    m_modify_args->GetPipeline()->Bind(command_buffer);
                    BindDescriptorSetscommand_buffer, 0, m_modify_args.Get());
                    m_modify_args->GetPipeline()->Dispatch(command_buffer, {1, 1, 1});
                    
                    m_indirect_buffer->InsertBarrier(command_buffer, ResourceState::INDIRECT_ARG);
                    m_build_info_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);

                    HYPERION_RETURN_OK;
                });
            }

            return commands.Execute(device);
        }
    ));

    WriteMipmaps;
}

void SparseVoxelOctree::WriteMipmaps()
{
    renderer::ComputePipeline::PushConstantData push_constants {
        .octree_data = {
            .num_fragments = m_voxelizer->NumFragments(),
            .voxel_grid_size = static_cast<UInt32>(m_voxelizer->voxel_map_size)
        }
    };

	const UInt32 fragment_group_x = group_x_64(m_voxelizer->NumFragments());

    auto commands = Engine::Get()->GetInstance()->GetSingleTimeCommands();

    commands.Push([&](CommandBuffer *command_buffer) {
        for (UInt32 i = 2; i <= m_voxelizer->octree_depth; i++) {
            push_constants.octree_data.mipmap_level = i;

            m_write_mipmaps->GetPipeline()->Bind(command_buffer, push_constants);
            BindDescriptorSetscommand_buffer, 0, m_write_mipmaps.Get());
            m_write_mipmaps->GetPipeline()->Dispatch(command_buffer, {fragment_group_x, 1, 1});

            if (i != m_voxelizer->octree_depth) {
                m_octree_buffer->InsertBarrier(command_buffer, ResourceState::UNORDERED_ACCESS);
            }
        }

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(commands.Execute(Engine::Get()->GetDevice()));
}

void SparseVoxelOctree::BindDescriptorSets(
    
    CommandBuffer *command_buffer,
    UInt frame_index,
    const ComputePipeline *pipeline
) const
{
    command_buffer->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        pipeline->GetPipeline(),
        m_descriptor_sets[frame_index].Get(),
        static_cast<DescriptorSet::Index>(0)
    );
}

} // namespace hyperion::v2
