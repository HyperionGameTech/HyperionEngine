#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include "rt/RendererRaytracingPipeline.hpp"
#include "rt/RendererAccelerationStructure.hpp"

#include <math/MathUtil.hpp>

namespace hyperion {
namespace renderer {

const decltype(DescriptorSet::mappings) DescriptorSet::mappings = {
    {
        DESCRIPTOR_SET_INDEX_GLOBAL,
        {
            {DescriptorKey::GBUFFER_TEXTURES,       0},
            {DescriptorKey::GBUFFER_DEPTH,          1},
            {DescriptorKey::GBUFFER_MIP_CHAIN,      2},
            {DescriptorKey::GBUFFER_DEPTH_SAMPLER,  3},
            {DescriptorKey::GBUFFER_SAMPLER,        4},

            {DescriptorKey::DEFERRED_RESULT,        5},

            {DescriptorKey::POST_FX_PRE_STACK,      8},
            {DescriptorKey::POST_FX_POST_STACK,     9},
            {DescriptorKey::POST_FX_UNIFORMS,      10},

            {DescriptorKey::SSR_UV_IMAGE,          12},
            {DescriptorKey::SSR_SAMPLE_IMAGE,      13},
            {DescriptorKey::SSR_RADIUS_IMAGE,      14},
            {DescriptorKey::SSR_BLUR_HOR_IMAGE,    15},
            {DescriptorKey::SSR_BLUR_VERT_IMAGE,   16},

            {DescriptorKey::SSR_UV_TEXTURE,        17},
            {DescriptorKey::SSR_SAMPLE_TEXTURE,    18},
            {DescriptorKey::SSR_RADIUS_TEXTURE,    19},
            {DescriptorKey::SSR_BLUR_HOR_TEXTURE,  20},
            {DescriptorKey::SSR_BLUR_VERT_TEXTURE, 21},
            {DescriptorKey::SSR_FINAL_TEXTURE,     22},

            {DescriptorKey::CUBEMAP_UNIFORMS,      24},
            {DescriptorKey::ENV_PROBE_TEXTURES,    25},
            {DescriptorKey::ENV_PROBES,            26},

            {DescriptorKey::VOXEL_IMAGE,           30},

            // result from depth pyramid generation
            {DescriptorKey::DEPTH_PYRAMID_RESULT,  36},

            {DescriptorKey::SSR_RESULT,            39},

            // sparse voxel octree buffer
            {DescriptorKey::SVO_BUFFER,            40},

            // combined result of AO (alpha channel) and SS GI (if applicable, in rgb)
            {DescriptorKey::SSAO_GI_RESULT,        41},
            // final UI image
            {DescriptorKey::UI_TEXTURE,            42},
            // motion vectors result
            {DescriptorKey::MOTION_VECTORS_RESULT, 43},

            // result from rt radiance image
            {DescriptorKey::RT_RADIANCE_RESULT,    45},
            // uniforms for RT probes
            {DescriptorKey::RT_PROBE_UNIFORMS,     46},
            // result from rt probes - irradiance
            {DescriptorKey::RT_IRRADIANCE_GRID,    47},
            // result from rt probes - depth
            {DescriptorKey::RT_DEPTH_GRID,         48},

            // result from temporal AA pass - temp, will be put into post fx chain
            {DescriptorKey::TEMPORAL_AA_RESULT,    50},
            
            // immediate drawing transforms
            {DescriptorKey::IMMEDIATE_DRAWS,       51},

            {DescriptorKey::DEFERRED_LIGHTING_AMBIENT, 55},
            {DescriptorKey::DEFERRED_LIGHTING_DIRECT,  56}
        }
    },
    {
        DESCRIPTOR_SET_INDEX_SCENE,
        {
            {DescriptorKey::SCENE_BUFFER,      0},
            {DescriptorKey::LIGHTS_BUFFER,     1},
            {DescriptorKey::SHADOW_MAPS,      12},
            {DescriptorKey::SHADOW_MATRICES,  13}
        }
    },
    {
        DESCRIPTOR_SET_INDEX_OBJECT,
        {
            {DescriptorKey::MATERIAL_BUFFER,  0},
            {DescriptorKey::OBJECT_BUFFER,    1},
            {DescriptorKey::SKELETON_BUFFER,  2},
            {DescriptorKey::ENTITY_INSTANCES, 3}
        }
    },
#if HYP_FEATURES_BINDLESS_TEXTURES
    {
        DESCRIPTOR_SET_INDEX_BINDLESS,
        {
            {DescriptorKey::TEXTURES, 0}
        }
    }
#else
    {
        DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES,
        {
            {DescriptorKey::SAMPLER, 0},
            {DescriptorKey::TEXTURES, 1}
        }
    }
#endif
};

const decltype(DescriptorSet::desired_indices) DescriptorSet::desired_indices = {
    { DESCRIPTOR_SET_INDEX_UNUSED,            0 },
    { DESCRIPTOR_SET_INDEX_GLOBAL,            1 },
    { DESCRIPTOR_SET_INDEX_SCENE,             2 },
    { DESCRIPTOR_SET_INDEX_VOXELIZER,         3 },
    { DESCRIPTOR_SET_INDEX_OBJECT,            4 },
#if HYP_FEATURES_BINDLESS_TEXTURES
    { DESCRIPTOR_SET_INDEX_BINDLESS,          5 },
    { DESCRIPTOR_SET_INDEX_RAYTRACING,        6 },
    { DESCRIPTOR_SET_INDEX_GLOBAL_FRAME_1,    7 },
    { DESCRIPTOR_SET_INDEX_SCENE_FRAME_1,     8 },
    { DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1,    9 },
    { DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1,  10 }
#else
    { DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, 5 },
    { DESCRIPTOR_SET_INDEX_GLOBAL_FRAME_1,    8 },
    { DESCRIPTOR_SET_INDEX_SCENE_FRAME_1,     6 },
    { DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1,    7 },
    { DESCRIPTOR_SET_INDEX_RAYTRACING,        9 } // todo: fix this crap up
#endif
};

DescriptorSet::Index DescriptorSet::GetBaseIndex(UInt index)
{
    if (index >= DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
        return DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES;
    }

    switch (index) {
    case DESCRIPTOR_SET_INDEX_GLOBAL_FRAME_1:
        return DESCRIPTOR_SET_INDEX_GLOBAL;
    case DESCRIPTOR_SET_INDEX_SCENE_FRAME_1:
        return DESCRIPTOR_SET_INDEX_SCENE;
    case DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1:
        return DESCRIPTOR_SET_INDEX_OBJECT;
    case DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1:
        return DESCRIPTOR_SET_INDEX_BINDLESS;
    }

    return Index(index);
}

DescriptorSet::Index DescriptorSet::GetPerFrameIndex(Index index, UInt frame_index)
{
    switch (GetBaseIndex(static_cast<UInt>(index))) {
    case DESCRIPTOR_SET_INDEX_GLOBAL:
        return frame_index ? DESCRIPTOR_SET_INDEX_GLOBAL_FRAME_1 : DESCRIPTOR_SET_INDEX_GLOBAL;
    case DESCRIPTOR_SET_INDEX_SCENE:
        return frame_index ? DESCRIPTOR_SET_INDEX_SCENE_FRAME_1 : DESCRIPTOR_SET_INDEX_SCENE;
    case DESCRIPTOR_SET_INDEX_OBJECT:
        return frame_index ? DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1 : DESCRIPTOR_SET_INDEX_OBJECT;
    case DESCRIPTOR_SET_INDEX_BINDLESS:
        return frame_index ? DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1 : DESCRIPTOR_SET_INDEX_BINDLESS;
    case DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES: {
        if (index == DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
            return index;
        }
        
        UInt frame_base = static_cast<UInt>(index);
        const auto index_offset = frame_base - (static_cast<UInt>(DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) + 1);

        if (index_offset % 2 != 0) { // it is for frame 1
            --frame_base;
        }

        return DescriptorSet::Index(frame_base + frame_index);
    }
    default:
        return index;
    }
}

DescriptorSet::Index DescriptorSet::GetPerFrameIndex(Index index, UInt instance_index, UInt frame_index)
{
    if (index == DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
        return DescriptorSet::Index(static_cast<UInt>(index) + 1 + (instance_index * 2) + frame_index);
    }

    return GetPerFrameIndex(index, frame_index);
}

int DescriptorSet::GetFrameIndex(UInt real_index)
{
    switch (GetBaseIndex(real_index)) {
    case DESCRIPTOR_SET_INDEX_GLOBAL:
        return real_index == DESCRIPTOR_SET_INDEX_GLOBAL_FRAME_1 ? 1 : 0;
    case DESCRIPTOR_SET_INDEX_SCENE:
        return real_index == DESCRIPTOR_SET_INDEX_SCENE_FRAME_1 ? 1 : 0;
    case DESCRIPTOR_SET_INDEX_OBJECT:
        return real_index == DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1 ? 1 : 0;
    case DESCRIPTOR_SET_INDEX_BINDLESS:
        return real_index == DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1 ? 1 : 0;
    case DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES: {
        if (real_index == DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
            return -1;
        }
        
        const auto index_offset = real_index - (static_cast<UInt>(DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) + 1);

        if (index_offset % 2 != 0) { // it is for frame 1
            return 1;
        }

        return 0;
    }
    default:
        return -1;
    }
}

UInt DescriptorSet::GetDesiredIndex(Index index)
{
    const auto it = desired_indices.Find(index);

    if (it == desired_indices.End()) {
        return static_cast<UInt>(index);
    }

    return it->second;
}

DescriptorSet::DescriptorSet()
    : m_set(VK_NULL_HANDLE),
      m_layout(VK_NULL_HANDLE),
      m_descriptor_pool(nullptr),
      m_state(DescriptorSetState::DESCRIPTOR_DIRTY),
      m_index(static_cast<Index>(-1)),
      m_real_index(~0u),
      m_bindless(false),
      m_is_created(false),
      m_is_standalone(true)
{
}

DescriptorSet::DescriptorSet(Index index, UInt real_index, bool bindless)
    : m_set(VK_NULL_HANDLE),
      m_layout(VK_NULL_HANDLE),
      m_descriptor_pool(nullptr),
      m_state(DescriptorSetState::DESCRIPTOR_DIRTY),
      m_index(index),
      m_real_index(real_index),
      m_bindless(bindless),
      m_is_created(false),
      m_is_standalone(false)
{
}

DescriptorSet::DescriptorSet(DescriptorSet &&other) noexcept
    : m_set(other.m_set),
      m_layout(other.m_layout),
      m_descriptor_pool(other.m_descriptor_pool),
      m_state(other.m_state),
      m_index(other.m_index),
      m_real_index(other.m_real_index),
      m_bindless(other.m_bindless),
      m_is_created(other.m_is_created),
      m_is_standalone(other.m_is_standalone)
{
    other.m_set = VK_NULL_HANDLE;
    other.m_layout = VK_NULL_HANDLE;
    other.m_descriptor_pool = nullptr;
    other.m_state = DescriptorSetState::DESCRIPTOR_DIRTY;
    other.m_index = static_cast<DescriptorSet::Index>(-1);
    other.m_real_index = ~0u;
    other.m_bindless = false;
    other.m_is_created = false;
    other.m_is_standalone = false;
}

DescriptorSet::~DescriptorSet()
{
    if (m_is_standalone) {
        AssertThrowMsg(m_layout == VK_NULL_HANDLE, "Layout not destroyed!");
    }

    AssertThrowMsg(m_set == VK_NULL_HANDLE, "Set not destroyed!");
}

Result DescriptorSet::Create(Device *device, DescriptorPool *pool)
{
    AssertThrow(pool != nullptr);
    AssertThrow(!m_is_created);
    AssertThrow(m_descriptor_bindings.Size() == m_descriptors.Size());
    
    m_descriptor_pool = pool;

    m_descriptor_writes.Clear();
    m_descriptor_writes.Reserve(m_descriptors.Size());

    for (SizeType i = 0; i < m_descriptors.Size(); i++) {
        auto &descriptor = m_descriptors[i];
        descriptor->m_descriptor_set = this;

        descriptor->Create(device, m_descriptor_bindings[i], m_descriptor_writes);
    }

    //build layout first
    VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.pBindings = m_descriptor_bindings.Data();
    layout_info.bindingCount = static_cast<UInt>(m_descriptor_bindings.Size());
    layout_info.flags = 0;

    constexpr VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    const std::vector<VkDescriptorBindingFlags> binding_flags(
        m_descriptor_bindings.Size(),
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | (IsBindless() ? bindless_flags : 0)
    );

    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    extended_info.bindingCount = static_cast<UInt>(binding_flags.size());
    extended_info.pBindingFlags = binding_flags.data();
    
    layout_info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_info.pNext = &extended_info;

    if (m_is_standalone) {
        // 'standalone' means we create our own descriptor set layout and manage it.
        if (vkCreateDescriptorSetLayout(device->GetDevice(), &layout_info, nullptr, &m_layout) != VK_SUCCESS) {
            return {Result::RENDERER_ERR, "Could not create descriptor set layout"};
        }
    } else {
        if (m_real_index == m_index) { // create a descriptor layout for the 'root' one aka not a template/copy of other.
            auto layout_result = pool->CreateDescriptorSetLayout(device, static_cast<UInt>(m_index), &layout_info, &m_layout);

            if (!layout_result) {
                DebugLog(LogType::Error, "Failed to create descriptor set layout! Message was: %s\n", layout_result.message);

                return layout_result;
            }
        } else { // reuse from template or base
            m_layout = pool->GetDescriptorSetLayout(static_cast<UInt>(GetIndex()));

            pool->SetDescriptorSetLayout(GetRealIndex(), m_layout);

            DebugLog(
                LogType::Debug,
                "Reuse descriptor set layout for descriptor set with index %u because base differs (%u)\n",
                static_cast<UInt>(GetRealIndex()),
                static_cast<UInt>(GetIndex())
            );
        }
    }

#if !HYP_FEATURES_BINDLESS_TEXTURES
    if (IsBindless()) {
        m_state = DescriptorSetState::DESCRIPTOR_CLEAN;
        HYPERION_RETURN_OK;
    }
#endif

    {
        auto allocate_result = pool->AllocateDescriptorSet(device, &m_layout, this);

        if (!allocate_result) {
            DebugLog(LogType::Error, "Failed to allocate descriptor set %lu! Message was: %s\n", static_cast<UInt>(m_index), allocate_result.message);

            return allocate_result;
        }
    }
    
    if (m_descriptor_writes.Any()) {
        for (auto &write : m_descriptor_writes) {
            write.dstSet = m_set;
        }

        vkUpdateDescriptorSets(
            device->GetDevice(),
            static_cast<UInt>(m_descriptor_writes.Size()),
            m_descriptor_writes.Data(),
            0,
            nullptr
        );

        m_descriptor_writes.Clear();
    }

    m_state = DescriptorSetState::DESCRIPTOR_CLEAN;

    for (auto &descriptor : m_descriptors) {
        descriptor->m_dirty_sub_descriptors = {};
    }

    m_is_created = true;

    HYPERION_RETURN_OK;
}

Result DescriptorSet::Destroy(Device *device)
{
    AssertThrow(m_descriptor_pool != nullptr);

    auto result = Result::OK;
    
    HYPERION_VK_PASS_ERRORS(
        vkFreeDescriptorSets(
            device->GetDevice(),
            m_descriptor_pool->GetHandle(),
            1,
            &m_set
        ),
        result
    );

    m_set = VK_NULL_HANDLE;

    if (m_is_standalone) {
        vkDestroyDescriptorSetLayout(
            device->GetDevice(),
            m_layout,
            nullptr
        );

        m_layout = VK_NULL_HANDLE;
    }
    
    m_descriptor_pool = nullptr;

    m_is_created = false;
    
    return result;
}

bool DescriptorSet::RemoveDescriptor(Descriptor *descriptor)
{
    AssertThrow(descriptor != nullptr);

    return RemoveDescriptor(descriptor->GetBinding());
}

bool DescriptorSet::RemoveDescriptor(DescriptorKey key)
{
    return RemoveDescriptor(DescriptorKeyToIndex(key));
}

bool DescriptorSet::RemoveDescriptor(UInt binding)
{
    const auto it = m_descriptors.FindIf([binding](const auto &item) {
        return item->GetBinding() == binding;
    });

    if (it == m_descriptors.End()) {
        return false;
    }

    m_descriptors.Erase(it);

    return true;
}

Descriptor *DescriptorSet::GetDescriptor(DescriptorKey key) const
{
    return GetDescriptor(DescriptorKeyToIndex(key));
}

Descriptor *DescriptorSet::GetDescriptor(UInt binding) const
{
    const auto it = m_descriptors.FindIf([binding](const auto &item) {
        return item->GetBinding() == binding;
    });

    if (it == m_descriptors.End()) {
        return nullptr;
    }

    return it->get();
}

UInt DescriptorSet::DescriptorKeyToIndex(DescriptorKey key) const
{
    auto index_it = mappings.find(m_index);
    AssertThrowMsg(index_it != mappings.end(), "Cannot add descriptor by key: descriptor set key has no mapping");

    auto key_it = index_it->second.find(key);
    AssertThrowMsg(key_it != index_it->second.end(), "Cannot add descriptor by key: descriptor key has no mapping");

    return key_it->second;
}

void DescriptorSet::ApplyUpdates(Device *device)
{
    if (m_state == DescriptorSetState::DESCRIPTOR_CLEAN) {
        return;
    }

    for (auto &descriptor : m_descriptors) {
        if (!descriptor->m_dirty_sub_descriptors) {
           continue;
        }

        descriptor->BuildUpdates(device, m_descriptor_writes);
        descriptor->m_dirty_sub_descriptors = { };
        descriptor->m_sub_descriptor_update_indices.Clear();
    }

    if (m_descriptor_writes.Any()) {
        for (VkWriteDescriptorSet &write : m_descriptor_writes) {
            write.dstSet = m_set;
        }

#ifdef HYP_LOG_DESCRIPTOR_SET_UPDATES
        DebugLog(
            LogType::Debug,
            "Update descriptor set: %llu writes\n",
            m_descriptor_writes.Size()
        );
#endif

        vkUpdateDescriptorSets(
            device->GetDevice(),
            static_cast<UInt>(m_descriptor_writes.Size()),
            m_descriptor_writes.Data(),
            0,
            nullptr
        );

        m_descriptor_writes.Clear();
    }

    m_state = DescriptorSetState::DESCRIPTOR_CLEAN;
}

const std::unordered_map<VkDescriptorType, UInt> DescriptorPool::items_per_set{
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
    {VK_DESCRIPTOR_TYPE_SAMPLER,                4096},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          4096},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          32},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         64},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         32},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32}
};

DescriptorPool::DescriptorPool()
    : m_descriptor_pool(nullptr),
      m_is_created(false)
{
}

DescriptorPool::~DescriptorPool()
{
    AssertThrowMsg(m_descriptor_pool == nullptr, "descriptor pool should have been destroyed!");
}

Result DescriptorPool::Create(Device *device)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(items_per_set.size());

    for (auto &it : items_per_set) {
        pool_sizes.push_back({
            it.first,
            it.second * 1000
        });
    }

    VkDescriptorPoolCreateInfo pool_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.maxSets = DescriptorSet::max_descriptor_sets;
    pool_info.poolSizeCount = static_cast<UInt>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    HYPERION_VK_CHECK_MSG(
        vkCreateDescriptorPool(device->GetDevice(), &pool_info, nullptr, &m_descriptor_pool),
        "Could not create descriptor pool!"
    );

#define HYP_DEBUG_LOG_LIMIT(limit_name) \
    DebugLog(\
        LogType::Debug, \
        "Limit " #limit_name ": %llu\n", \
        device->GetFeatures().GetPhysicalDeviceProperties().limits.limit_name \
    )

    HYP_DEBUG_LOG_LIMIT(maxMemoryAllocationCount);
    HYP_DEBUG_LOG_LIMIT(maxSamplerAllocationCount);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetSamplers);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetSampledImages);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetStorageImages);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetInputAttachments);
    HYP_DEBUG_LOG_LIMIT(maxUniformBufferRange);
    HYP_DEBUG_LOG_LIMIT(maxStorageBufferRange);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetUniformBuffers);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetUniformBuffersDynamic);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetStorageBuffers);
    HYP_DEBUG_LOG_LIMIT(maxDescriptorSetStorageBuffersDynamic);
    HYP_DEBUG_LOG_LIMIT(maxPerStageDescriptorSamplers);

#undef HYP_DEBUG_LOG_LIMIT

    m_is_created = true;

    HYPERION_RETURN_OK;
}

Result DescriptorPool::Destroy(Device *device)
{
    auto result = Result::OK;

    /* Destroy set layouts */
    for (auto &it : m_descriptor_set_layouts.Values()) {
        vkDestroyDescriptorSetLayout(device->GetDevice(), it, nullptr);
    }

    m_descriptor_set_layouts.Clear();

    /* Destroy sets */
    for (auto &set : m_descriptor_sets) {
        if (set != nullptr) {
            HYPERION_PASS_ERRORS(set->Destroy(device), result);
        }
    }
    
    m_descriptor_sets.Clear();

    /* Destroy pool */
    vkDestroyDescriptorPool(device->GetDevice(), m_descriptor_pool, nullptr);
    m_descriptor_pool = nullptr;

    m_is_created = false;

    return result;
}

DescriptorSet *DescriptorPool::AddDescriptorSet(
    Device *device,
    std::unique_ptr<DescriptorSet> &&descriptor_set,
    bool add_immediately
)
{
    AssertThrow(descriptor_set != nullptr);
    const UInt index = descriptor_set->GetRealIndex();

    if (add_immediately) {
        if (index >= m_descriptor_sets.Size()) {
            m_descriptor_sets.Resize(index + 1);
        }

        if (index < m_descriptor_sets.Size()) {
            AssertThrowMsg(
                m_descriptor_sets[index] == nullptr,
                "Descriptor set at index %u not nullptr! This would cause it to be overwritten.",
                index
            );
        }

        m_descriptor_sets[index] = std::move(descriptor_set);

        return m_descriptor_sets[index].get();
    }

    const auto descriptor_set_frame_index = DescriptorSet::GetFrameIndex(index);
    AssertThrow(descriptor_set_frame_index >= 0 && descriptor_set_frame_index < max_frames_in_flight);

    m_descriptor_sets_pending_addition[descriptor_set_frame_index].PushBack(std::move(descriptor_set));

    return m_descriptor_sets_pending_addition[descriptor_set_frame_index].Back().get();
}

void DescriptorPool::RemoveDescriptorSet(DescriptorSet *descriptor_set)
{
    AssertThrow(descriptor_set != nullptr);

    RemoveDescriptorSet(descriptor_set->GetRealIndex());
}

void DescriptorPool::RemoveDescriptorSet(UInt index)
{
    const UInt queue_index = DescriptorSet::GetFrameIndex(index);

    // look through the list of items pending addition, remove from there
    for (auto it = m_descriptor_sets_pending_addition[queue_index].Begin(); it != m_descriptor_sets_pending_addition[queue_index].End();) {
        auto &descriptor_set = (*it);
        AssertThrow(descriptor_set != nullptr);
        
        if (descriptor_set->GetRealIndex() == index) {
            if (m_descriptor_sets_pending_destruction[queue_index].Contains(descriptor_set->GetRealIndex())) {
                DebugLog(
                    LogType::Warn,
                    "Descriptor set at index %u is already queued for removal",
                    index
                );

                return;
            }

            // if (descriptor_set->IsCreated()) {
            //     HYPERION_ASSERT_RESULT(descriptor_set->Destroy(device));
            // }

            it = m_descriptor_sets_pending_addition[queue_index].Erase(it);

            // if (IsCreated()) { // creating at runtime, after descriptor sets all created
            //     HYPERION_BUBBLE_ERRORS((*it)->Create(
            //         Engine::Get()->GetGPUDevice(),
            //         &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            //     ));
            // }

            return; // found
        } else {
            ++it;
        }
    }

    AssertThrowMsg(
        index < m_descriptor_sets.Size(),
        "Attempt to remove descriptor set at index %u but it is out of bounds (%llu)",
        index,
        m_descriptor_sets.Size()
    );

    auto &descriptor_set = m_descriptor_sets[index];
    AssertThrowMsg(
        descriptor_set != nullptr,
        "Attempt to remove descriptor set at index %u but it is nullptr",
        index
    );

    if (m_descriptor_sets_pending_destruction[queue_index].Contains(descriptor_set->GetRealIndex())) {
        DebugLog(
            LogType::Warn,
            "Descriptor set at index %u is already queued for removal",
            index
        );

        return;
    }

    m_descriptor_sets_pending_destruction[queue_index].Push(descriptor_set->GetRealIndex());
}

Result DescriptorPool::DestroyPendingDescriptorSets(Device *device, UInt frame_index)
{
    auto &descriptor_set_queue = m_descriptor_sets_pending_destruction[frame_index];

    while (descriptor_set_queue.Any()) {
        const auto index = descriptor_set_queue.Front();
        AssertThrow(index < m_descriptor_sets.Size());

        auto &item = m_descriptor_sets[index];
        AssertThrow(item != nullptr);
    
        if (item->IsCreated()) {
            HYPERION_BUBBLE_ERRORS(item->Destroy(device));
        }

        m_descriptor_sets[index].reset();

        descriptor_set_queue.Pop();
    }

    HYPERION_RETURN_OK;
}

Result DescriptorPool::AddPendingDescriptorSets(Device *device, UInt frame_index)
{
    while (m_descriptor_sets_pending_addition[frame_index].Any()) {
        auto descriptor_set = m_descriptor_sets_pending_addition[frame_index].PopFront();

        const UInt index = descriptor_set->GetRealIndex();
        
        if (index >= m_descriptor_sets.Size()) {
            m_descriptor_sets.Resize(index + 1);
        }

        if (index < m_descriptor_sets.Size()) {
            AssertThrowMsg(
                m_descriptor_sets[index] == nullptr,
                "Descriptor set at index %u not nullptr! This would cause it to be overwritten.",
                index
            );
        }

        m_descriptor_sets[index] = std::move(descriptor_set);
    }

    HYPERION_RETURN_OK;
}

Result DescriptorPool::UpdateDescriptorSets(Device *device, UInt frame_index)
{
    for (auto &descriptor_set : m_descriptor_sets) {
        if (descriptor_set == nullptr) {
            continue;
        }

        if (descriptor_set->GetState() == DescriptorSetState::DESCRIPTOR_CLEAN) {
            continue;
        }

        const auto slot  = DescriptorSet::GetBaseIndex(descriptor_set->GetIndex());

#if HYP_FEATURES_BINDLESS_TEXTURES
        if (slot == DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
            continue;
        }
#else
        if (slot == DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS) {
            continue;
        }
#endif

        const auto descriptor_set_frame_index = DescriptorSet::GetFrameIndex(descriptor_set->GetRealIndex());

        if (descriptor_set_frame_index == static_cast<int>(frame_index) || descriptor_set_frame_index == -1) {
            descriptor_set->ApplyUpdates(device);
        }
    }

    HYPERION_RETURN_OK;
}

Result DescriptorPool::CreateDescriptorSets(Device *device)
{
    AssertThrow(m_is_created);

    auto result = Result::OK;

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        DestroyPendingDescriptorSets(device, frame_index);
        AddPendingDescriptorSets(device, frame_index);
    }

    UInt descriptor_set_index = 0;

    for (auto &descriptor_set : m_descriptor_sets) {
        if (descriptor_set == nullptr) {
            DebugLog(LogType::Warn, "Descriptor set %u null, skipping...\n", descriptor_set_index);

            descriptor_set_index++;

            continue;
        }

        if (descriptor_set->IsCreated()) {
            continue;
        }

        HYPERION_PASS_ERRORS(
            descriptor_set->Create(device, this),
            result
        );

        descriptor_set_index++;
    }

    return result;
}

Result DescriptorPool::Bind(
    Device *device,
    CommandBuffer *cmd,
    GraphicsPipeline *pipeline,
    const DescriptorSetBinding &binding
) const
{
    BindDescriptorSets(
        device,
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline,
        binding
    );

    HYPERION_RETURN_OK;
}

Result DescriptorPool::Bind(
    Device *device,
    CommandBuffer *cmd,
    ComputePipeline *pipeline,
    const DescriptorSetBinding &binding
) const
{
    BindDescriptorSets(
        device,
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline,
        binding
    );

    HYPERION_RETURN_OK;
}

Result DescriptorPool::Bind(
    Device *device,
    CommandBuffer *cmd,
    RaytracingPipeline *pipeline,
    const DescriptorSetBinding &binding
) const
{
    BindDescriptorSets(
        device,
        cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline,
        binding
    );

    HYPERION_RETURN_OK;
}

void DescriptorPool::BindDescriptorSets(
    Device *device,
    CommandBuffer *cmd,
    VkPipelineBindPoint bind_point,
    Pipeline *pipeline,
    const DescriptorSetBinding &binding
) const
{
    const auto device_max_bound_descriptor_sets = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    const size_t max_bound_descriptor_sets = DescriptorSet::max_bound_descriptor_sets != 0
        ? MathUtil::Min(
            DescriptorSet::max_bound_descriptor_sets,
            device_max_bound_descriptor_sets
        )
        : device_max_bound_descriptor_sets;

    const auto set_index = static_cast<UInt>(binding.declaration.set);
    const auto binding_index = DescriptorSet::GetDesiredIndex(binding.locations.binding);

    AssertThrowMsg(
        binding.declaration.count <= max_bound_descriptor_sets,
        "Requested binding of %d descriptor sets, but maximum bound is %d",
        binding.declaration.count,
        max_bound_descriptor_sets
    );

    AssertThrowMsg(
        set_index < m_descriptor_sets.Size(),
        "Attempt to bind invalid descriptor set (%u) (at index %u) -- out of bounds (max is %llu)\n",
        static_cast<UInt>(binding.declaration.set),
        set_index,
        m_descriptor_sets.Size()
    );

    auto &bind_set = m_descriptor_sets[set_index];

    AssertThrowMsg(
        bind_set != nullptr,
        "Attempt to bind invalid descriptor set %u (at index %u) -- set is null\n",
        static_cast<UInt>(binding.declaration.set),
        set_index
    );

    const auto handle = bind_set->m_set;

    vkCmdBindDescriptorSets(
        cmd->GetCommandBuffer(),
        bind_point,
        pipeline->layout,
        binding_index,
        binding.declaration.count,
        &handle,
        static_cast<UInt>(binding.offsets.offsets.size()),
        binding.offsets.offsets.data()
    );
}

Result DescriptorPool::CreateDescriptorSetLayout(Device *device, UInt index, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out)
{
    if (vkCreateDescriptorSetLayout(device->GetDevice(), layout_create_info, nullptr, out) != VK_SUCCESS) {
        return {Result::RENDERER_ERR, "Could not create descriptor set layout"};
    }
    
    m_descriptor_set_layouts.Insert(index, *out);

    HYPERION_RETURN_OK;
}

Result DescriptorPool::DestroyDescriptorSetLayout(Device *device, UInt index)
{
    auto it = m_descriptor_set_layouts.Find(index);

    if (it == m_descriptor_set_layouts.End()) {
        return { Result::RENDERER_ERR, "Could not destroy descriptor set layout; not found in list" };
    }

    vkDestroyDescriptorSetLayout(device->GetDevice(), it->second, nullptr);

    m_descriptor_set_layouts.Erase(it);

    HYPERION_RETURN_OK;
}

VkDescriptorSetLayout DescriptorPool::GetDescriptorSetLayout(UInt index)
{
    return m_descriptor_set_layouts.At(index);
}

void DescriptorPool::SetDescriptorSetLayout(UInt index, VkDescriptorSetLayout layout)
{
    m_descriptor_set_layouts.Insert(index, layout);
}

Result DescriptorPool::AllocateDescriptorSet(
    Device *device,
    VkDescriptorSetLayout *layout,
    DescriptorSet *out
)
{
    AssertThrowMsg(
        m_descriptor_pool != VK_NULL_HANDLE,
        "The descriptor pool is not yet created."
    );

    VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.pSetLayouts = layout;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;

    // bindless stuff
    constexpr UInt max_bindings = DescriptorSet::max_bindless_resources - 1;

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
    count_info.descriptorSetCount = 1;
    // This number is the max allocatable count
    count_info.pDescriptorCounts = &max_bindings;

    if (out->IsBindless()) {
        alloc_info.pNext = &count_info;
    }

    const VkResult alloc_result = vkAllocateDescriptorSets(device->GetDevice(), &alloc_info, &out->m_set);

    switch (alloc_result) {
    case VK_SUCCESS: return Result::OK;
    case VK_ERROR_FRAGMENTED_POOL:
        return { Result::RENDERER_ERR_NEEDS_REALLOCATION, "Fragmented pool", alloc_result };
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return { Result::RENDERER_ERR_NEEDS_REALLOCATION, "Out of pool memory", alloc_result };
    default:
        return { Result::RENDERER_ERR, "Unknown error (check error code)", alloc_result };
    }
}

Descriptor::Descriptor(UInt binding, DescriptorType descriptor_type)
    : m_binding(binding),
      m_descriptor_type(descriptor_type),
      m_descriptor_set(nullptr)
{
}

Descriptor::~Descriptor() = default;

void Descriptor::Create(
    Device *device,
    VkDescriptorSetLayoutBinding &binding,
    Array<VkWriteDescriptorSet> &writes
)
{
    AssertThrow(m_descriptor_set != nullptr);

    const auto descriptor_type = ToVkDescriptorType(m_descriptor_type);

    m_sub_descriptor_update_indices.Clear();

    auto descriptor_count = static_cast<UInt>(m_sub_descriptors.Size());

    if (m_descriptor_set->IsBindless()) {
        descriptor_count = DescriptorSet::max_bindless_resources;
    } else if (
        m_descriptor_set->IsTemplate()
        && m_descriptor_set->GetIndex() == DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
        && m_binding == m_descriptor_set->DescriptorKeyToIndex(DescriptorKey::TEXTURES)
    ) {
        descriptor_count = DescriptorSet::max_material_texture_samplers;
    }
    
    binding.descriptorCount = descriptor_count;
    binding.descriptorType = descriptor_type;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = VK_SHADER_STAGE_ALL;
    binding.binding = m_binding;

    if (descriptor_count == 0) {
        DebugLog(
            LogType::Debug,
            "Descriptor has 0 elements, returning from Create() without pushing any writes\n"
        );

        return;
    }

    for (auto &it : m_sub_descriptors) {
        auto &sub_descriptor = it.second;

        UpdateSubDescriptorBuffer(
            sub_descriptor,
            sub_descriptor.buffer_info,
            sub_descriptor.image_info,
            sub_descriptor.acceleration_structure_info
        );
        
        VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
        write.dstBinding = m_binding;
        write.dstArrayElement = sub_descriptor.element_index;
        write.descriptorCount = 1;
        write.descriptorType = descriptor_type;
        write.pBufferInfo = &sub_descriptor.buffer_info;
        write.pImageInfo = &sub_descriptor.image_info;
        
        if (m_descriptor_type == DescriptorType::ACCELERATION_STRUCTURE) {
            write.pNext = &sub_descriptor.acceleration_structure_info;
        }

        writes.PushBack(write);
    }
}

void Descriptor::BuildUpdates(Device *, Array<VkWriteDescriptorSet> &writes)
{
    const auto descriptor_type = ToVkDescriptorType(m_descriptor_type);

    for (auto &it : m_sub_descriptors) {
        auto &sub_descriptor = it.second;

        UpdateSubDescriptorBuffer(
            sub_descriptor,
            sub_descriptor.buffer_info,
            sub_descriptor.image_info,
            sub_descriptor.acceleration_structure_info
        );
        
        VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
        write.dstBinding = m_binding;
        write.dstArrayElement = sub_descriptor.element_index;
        write.descriptorCount = 1;
        write.descriptorType = descriptor_type;
        write.pBufferInfo = &sub_descriptor.buffer_info;
        write.pImageInfo = &sub_descriptor.image_info;
        
        if (m_descriptor_type == DescriptorType::ACCELERATION_STRUCTURE) {
            write.pNext = &sub_descriptor.acceleration_structure_info;
        }

        writes.PushBack(write);
    }
}

void Descriptor::UpdateSubDescriptorBuffer(
    const SubDescriptor &sub_descriptor,
    VkDescriptorBufferInfo &out_buffer,
    VkDescriptorImageInfo &out_image,
    VkWriteDescriptorSetAccelerationStructureKHR &out_acceleration_structure
) const
{
    switch (m_descriptor_type) {
    case DescriptorType::UNIFORM_BUFFER: /* fallthrough */
    case DescriptorType::UNIFORM_BUFFER_DYNAMIC:
    case DescriptorType::STORAGE_BUFFER:
    case DescriptorType::STORAGE_BUFFER_DYNAMIC:
        AssertThrow(sub_descriptor.buffer != nullptr);
        AssertThrow(sub_descriptor.buffer->buffer != nullptr);

        out_buffer = {
            .buffer = sub_descriptor.buffer->buffer,
            .offset = 0,
            .range = sub_descriptor.range != 0
                ? sub_descriptor.range
                : sub_descriptor.buffer->size
        };

        break;
    case DescriptorType::IMAGE:
        AssertThrow(sub_descriptor.image_view != nullptr);
        AssertThrow(sub_descriptor.image_view->GetImageView() != nullptr);

        out_image = {
            .sampler     = VK_NULL_HANDLE,
            .imageView   = sub_descriptor.image_view->GetImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        break;
    case DescriptorType::SAMPLER:
        AssertThrow(sub_descriptor.sampler != nullptr);
        AssertThrow(sub_descriptor.sampler->GetSampler() != nullptr);

        out_image = {
            .sampler     = sub_descriptor.sampler->GetSampler(),
            .imageView   = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        break;
    case DescriptorType::IMAGE_SAMPLER:
        AssertThrow(sub_descriptor.image_view != nullptr);
        AssertThrow(sub_descriptor.image_view->GetImageView() != nullptr);

        AssertThrow(sub_descriptor.sampler != nullptr);
        AssertThrow(sub_descriptor.sampler->GetSampler() != nullptr);

        out_image = {
            .sampler     = sub_descriptor.sampler->GetSampler(),
            .imageView   = sub_descriptor.image_view->GetImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        break;
    case DescriptorType::IMAGE_STORAGE:
        AssertThrow(sub_descriptor.image_view != nullptr);
        AssertThrow(sub_descriptor.image_view->GetImageView() != nullptr);

        out_image = {
            .sampler     = VK_NULL_HANDLE,
            .imageView   = sub_descriptor.image_view->GetImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        break;
    case DescriptorType::ACCELERATION_STRUCTURE:
        AssertThrow(sub_descriptor.acceleration_structure != nullptr);
        AssertThrow(sub_descriptor.acceleration_structure->GetAccelerationStructure() != nullptr);

        out_acceleration_structure = {
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &sub_descriptor.acceleration_structure->GetAccelerationStructure()
        };

        break;
    default:
        AssertThrowMsg(false, "unhandled descriptor type");
    }
}

UInt Descriptor::SetSubDescriptor(SubDescriptor &&sub_descriptor)
{
    sub_descriptor.valid = true;

    if (sub_descriptor.element_index == ~0u) {
        sub_descriptor.element_index = m_sub_descriptors.Empty() ? 0 : m_sub_descriptors.Back().first + 1;
    }

    const auto element_index = sub_descriptor.element_index;

    m_sub_descriptors[element_index] = sub_descriptor;

    MarkDirty(element_index);

    return element_index;
}

void Descriptor::RemoveSubDescriptor(UInt index)
{
    const auto it = m_sub_descriptors.Find(index);
    AssertThrow(it != m_sub_descriptors.End());
    
    m_sub_descriptors.Erase(it);

    const auto update_it = m_sub_descriptor_update_indices.Find(index);

    if (update_it != m_sub_descriptor_update_indices.End()) {
        m_sub_descriptor_update_indices.Erase(update_it);
    }
}

void Descriptor::MarkDirty(UInt sub_descriptor_index)
{
    m_sub_descriptor_update_indices.PushBack(sub_descriptor_index);

    m_dirty_sub_descriptors |= {sub_descriptor_index, sub_descriptor_index + 1};

    if (m_descriptor_set != nullptr) {
        m_descriptor_set->m_state = DescriptorSetState::DESCRIPTOR_DIRTY;
    }
}

VkDescriptorType Descriptor::ToVkDescriptorType(DescriptorType descriptor_type)
{
    switch (descriptor_type) {
    case DescriptorType::UNIFORM_BUFFER:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorType::UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case DescriptorType::STORAGE_BUFFER:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorType::STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case DescriptorType::IMAGE:                  return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorType::SAMPLER:                return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorType::IMAGE_SAMPLER:          return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorType::IMAGE_STORAGE:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorType::ACCELERATION_STRUCTURE: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type");
    }
}


} // namespace renderer
} // namespace hyperion
