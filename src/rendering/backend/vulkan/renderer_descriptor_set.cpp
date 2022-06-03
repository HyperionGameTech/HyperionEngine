#include "renderer_descriptor_set.h"
#include "renderer_command_buffer.h"
#include "renderer_graphics_pipeline.h"
#include "renderer_compute_pipeline.h"
#include "renderer_buffer.h"
#include "renderer_features.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "rt/renderer_raytracing_pipeline.h"
#include "rt/renderer_acceleration_structure.h"

#include <math/math_util.h>

namespace hyperion {
namespace renderer {

const decltype(DescriptorSet::mappings) DescriptorSet::mappings = {
    {
        DESCRIPTOR_SET_INDEX_GLOBAL,
        {
            {DescriptorKey::GBUFFER_TEXTURES,   0},
            {DescriptorKey::GBUFFER_DEPTH,      1},
            {DescriptorKey::DEFERRED_RESULT,    4},
            {DescriptorKey::POST_FX_PRE_STACK,  8},
            {DescriptorKey::POST_FX_POST_STACK, 9},
            {DescriptorKey::POST_FX_UNIFORMS,  10}
        }
    },
    {
        DESCRIPTOR_SET_INDEX_SCENE,
        {
            {DescriptorKey::SCENE_BUFFER,     0},
            {DescriptorKey::LIGHTS_BUFFER,    1},
            {DescriptorKey::SHADOW_MAPS,     12},
            {DescriptorKey::SHADOW_MATRICES, 13}
        }
    },
    {
        DESCRIPTOR_SET_INDEX_OBJECT,
        {
            {DescriptorKey::MATERIAL_BUFFER,  0},
            {DescriptorKey::OBJECT_BUFFER,    1},
            {DescriptorKey::SKELETON_BUFFER,  2}
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
            {DescriptorKey::TEXTURES, 0}
        }
    }
#endif
};

const decltype(DescriptorSet::desired_indices) DescriptorSet::desired_indices = {
    { DESCRIPTOR_SET_INDEX_UNUSED,            0 },
    { DESCRIPTOR_SET_INDEX_GLOBAL,            1 },
    { DESCRIPTOR_SET_INDEX_SCENE,             2 },
    { DESCRIPTOR_SET_INDEX_OBJECT,            3 },
    { DESCRIPTOR_SET_INDEX_SCENE_FRAME_1,     4 },
    { DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1,    5 },
#if HYP_FEATURES_BINDLESS_TEXTURES
    { DESCRIPTOR_SET_INDEX_BINDLESS,          6 },
    { DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1,  7 }, 
    { DESCRIPTOR_SET_INDEX_VOXELIZER,         8 },
    { DESCRIPTOR_SET_INDEX_RAYTRACING,        9 }
#else
    { DESCRIPTOR_SET_INDEX_VOXELIZER,         6 },
    { DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, 7 },
    { DESCRIPTOR_SET_INDEX_RAYTRACING,        8 } // todo: fix this crap up
#endif
};

DescriptorSet::Index DescriptorSet::GetBaseIndex(uint index)
{
    if (index >= DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
        return DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES;
    }

    switch (index) {
    case DESCRIPTOR_SET_INDEX_SCENE_FRAME_1:
        return DESCRIPTOR_SET_INDEX_SCENE;
    case DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1:
        return DESCRIPTOR_SET_INDEX_OBJECT;
    case DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1:
        return DESCRIPTOR_SET_INDEX_BINDLESS;
    }

    return Index(index);
}

DescriptorSet::Index DescriptorSet::GetPerFrameIndex(Index index, uint frame_index)
{
    if (frame_index == 0) {
        return GetBaseIndex(uint(index));
    }

    switch (index) {
    case DESCRIPTOR_SET_INDEX_SCENE:
        return DESCRIPTOR_SET_INDEX_SCENE_FRAME_1;
    case DESCRIPTOR_SET_INDEX_OBJECT:
        return DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1;
    case DESCRIPTOR_SET_INDEX_BINDLESS:
        return DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1;
    }

    return index;
}

DescriptorSet::Index DescriptorSet::GetPerFrameIndex(Index index, uint instance_index, uint frame_index)
{
    if (index == DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
        return DescriptorSet::Index(uint(index) + 1 + (instance_index * 2) + frame_index);
    }

    return GetPerFrameIndex(index, frame_index);
}

uint DescriptorSet::GetDesiredIndex(Index index)
{
    const auto it = desired_indices.find(index);

    if (it == desired_indices.end()) {
        return static_cast<uint>(index);
    }

    return it->second;
}

DescriptorSet::DescriptorSet(Index index, uint real_index, bool bindless)
    : m_set(VK_NULL_HANDLE),
      m_state(DescriptorSetState::DESCRIPTOR_DIRTY),
      m_index(index),
      m_real_index(real_index),
      m_bindless(bindless)
      
{
}

DescriptorSet::~DescriptorSet()
{
}

Result DescriptorSet::Create(Device *device, DescriptorPool *pool)
{
    AssertThrow(m_descriptor_bindings.size() == m_descriptors.size());

    m_descriptor_writes.clear();
    m_descriptor_writes.reserve(m_descriptors.size());

    for (size_t i = 0; i < m_descriptors.size(); i++) {
        auto &descriptor = m_descriptors[i];
        descriptor->m_descriptor_set = this;

        descriptor->Create(device, m_descriptor_bindings[i], m_descriptor_writes);
    }

    DebugLog(LogType::Debug, "\t%lu descriptors\n", m_descriptors.size());

    for (auto &it : m_descriptors) {
        DebugLog(LogType::Debug, "\t\t%lu sub descriptors\n", it->GetSubDescriptors().Size());

        for (auto &it : it->GetSubDescriptors()) {
            DebugLog(LogType::Debug, "\t\t\tidx: %lu\n", it.second.element_index);
        }
    }

    //build layout first
    VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.pBindings    = m_descriptor_bindings.data();
    layout_info.bindingCount = static_cast<uint32_t>(m_descriptor_bindings.size());
    layout_info.flags        = 0;

    constexpr VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    const std::vector<VkDescriptorBindingFlags> binding_flags(
        m_descriptor_bindings.size(),
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | (IsBindless() ? bindless_flags : 0)
    );

    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    extended_info.bindingCount  = static_cast<uint32_t>(binding_flags.size());
    extended_info.pBindingFlags = binding_flags.data();
    
    layout_info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_info.pNext = &extended_info;

    VkDescriptorSetLayout layout{};

    DebugLog(LogType::Debug, "%d  num desc set layouts before adding %llu\n", int(m_real_index), pool->GetDescriptorSetLayouts().Size());

    if (m_real_index == m_index) { // otherwise no need to create layout
        auto layout_result = pool->CreateDescriptorSetLayout(device, static_cast<uint>(m_index), &layout_info, &layout);

        if (!layout_result) {
            DebugLog(LogType::Error, "Failed to create descriptor set layout! Message was: %s\n", layout_result.message);

            return layout_result;
        }
    
        //if (IsTemplate()) {
        //    HYPERION_RETURN_OK;
        //}
    } else { // reuse from template or base
        layout = pool->GetDescriptorSetLayout(static_cast<uint>(GetIndex()));

        pool->SetDescriptorSetLayout(GetRealIndex(), layout);

        DebugLog(
            LogType::Debug,
            "Reuse descriptor set layout for descriptor set with index %u because base differs (%u)\n",
            static_cast<uint>(GetRealIndex()),
            static_cast<uint>(GetIndex())
        );
    }

#if !HYP_FEATURES_BINDLESS_TEXTURES
    if (IsBindless()) {
        m_state = DescriptorSetState::DESCRIPTOR_CLEAN;
        HYPERION_RETURN_OK;
    }
#endif

    {
        auto allocate_result = pool->AllocateDescriptorSet(device, &layout, this);

        if (!allocate_result) {
            DebugLog(LogType::Error, "Failed to allocate descriptor set %lu! Message was: %s\n", uint32_t(m_index), allocate_result.message);

            return allocate_result;
        }
    }
    
    if (!m_descriptor_writes.empty()) {
        for (auto &write : m_descriptor_writes) {
            write.dstSet = m_set;
        }

        vkUpdateDescriptorSets(device->GetDevice(), uint32_t(m_descriptor_writes.size()), m_descriptor_writes.data(), 0, nullptr);

        m_descriptor_writes.clear();
    }

    m_state = DescriptorSetState::DESCRIPTOR_CLEAN;

    for (auto &descriptor : m_descriptors) {
        descriptor->m_dirty_sub_descriptors = {};
    }

    HYPERION_RETURN_OK;
}

Result DescriptorSet::Destroy(Device *device)
{
    HYPERION_RETURN_OK;
}

void DescriptorSet::ApplyUpdates(Device *device)
{
    for (size_t i = 0; i < m_descriptors.size(); i++) {
        auto &descriptor = m_descriptors[i];

        ///if (!descriptor->m_dirty_sub_descriptors) {
        //    continue;
        //}

        descriptor->BuildUpdates(device, m_descriptor_writes);
    }

    if (!m_descriptor_writes.empty()) {
        for (VkWriteDescriptorSet &write : m_descriptor_writes) {
            write.dstSet = m_set;
        }

        vkUpdateDescriptorSets(device->GetDevice(), uint32_t(m_descriptor_writes.size()), m_descriptor_writes.data(), 0, nullptr);

        m_descriptor_writes.clear();
    }
}

const std::unordered_map<VkDescriptorType, size_t> DescriptorPool::items_per_set{
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
    {VK_DESCRIPTOR_TYPE_SAMPLER,                4096},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096}, /* sampling imageviews in shader */
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          32},          /* imageStore, imageLoad etc */
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         64},         /* standard uniform buffer */
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         32},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32}
};

DescriptorPool::DescriptorPool()
    : m_descriptor_pool(nullptr),
      m_descriptor_sets_view{},
      m_is_created(false)
{
}

DescriptorPool::~DescriptorPool()
{
    AssertExitMsg(m_descriptor_pool == nullptr, "descriptor pool should have been destroyed!");
}

Result DescriptorPool::Create(Device *device)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(items_per_set.size());

    for (auto &it : items_per_set) {
        pool_sizes.push_back({
            it.first,
            uint32_t(it.second * 1000)
        });
    }

    VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.maxSets       = DescriptorSet::max_descriptor_sets;
    pool_info.poolSizeCount = uint32_t(pool_sizes.size());
    pool_info.pPoolSizes    = pool_sizes.data();

    HYPERION_VK_CHECK_MSG(
        vkCreateDescriptorPool(device->GetDevice(), &pool_info, nullptr, &m_descriptor_pool),
        "Could not create descriptor pool!"
    );

#define HYP_DEBUG_LOG_LIMIT(limit_name) \
    DebugLog( \
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

    std::vector<std::pair<uint, VkDescriptorSet>> descriptor_set_views_indices;
    descriptor_set_views_indices.reserve(m_descriptor_sets.size());

    uint descriptor_set_index = 0;

    for (auto &descriptor_set : m_descriptor_sets) {
        if (descriptor_set == nullptr) {
            DebugLog(LogType::Warn, "Descriptor set %u null, skipping...\n", descriptor_set_index);

            descriptor_set_index++;

            continue;
        }

        auto allocate_result = descriptor_set->Create(device, this);

        if (!allocate_result) {
            device->DebugLogAllocatorStats();

            DebugLog(
                LogType::Error,
                "Failed to allocate descriptor set %llu!\n",
                descriptor_set_index
            );

            return allocate_result;
        }

        auto it = std::find_if(
            descriptor_set_views_indices.begin(),
            descriptor_set_views_indices.end(),
            [&](const auto &item) {
                return item.first == descriptor_set->GetDesiredIndex();
            }
        );

        AssertThrowMsg(it == descriptor_set_views_indices.end(), "Duplicate descriptor set index");

        DebugLog(LogType::Debug, "Fake index %u -> %u\n", descriptor_set_index, descriptor_set->GetDesiredIndex());

        descriptor_set_views_indices.push_back(std::make_pair(
            descriptor_set->GetDesiredIndex(),
            descriptor_set->m_set
        ));

        descriptor_set_index++;
    }

    /*std::sort(
        descriptor_set_views_indices.begin(),
        descriptor_set_views_indices.end(),
        [](const auto &lhs, const auto &rhs) {

        }
    )

    m_descriptor_sets_view.resize(descriptor_set_views_indices.size());

    for (size_t i = 0; i < descriptor_set_views_indices.size(); i++) {
        m_descriptor_sets_view[i] = descriptor_set_views_indices[i].second;
    }*/

    m_descriptor_sets_view.clear();

    for (auto &it : descriptor_set_views_indices) {
        AssertThrowMsg(
            it.second != nullptr,
            "Descriptor set at index %u  = %p\n",
            it.first,
            it.second
        );

        AssertThrowMsg(
            it.second != nullptr,
            "Descriptor set at index %u is missing",
            it.first
        );

        if (m_descriptor_sets_view.size() <= it.first) {
            m_descriptor_sets_view.resize(it.first + 1);
        }

        m_descriptor_sets_view[it.first] = it.second;
    }

    m_is_created = true;

    DebugLog(
        LogType::Debug,
        "All main descriptor sets created\n"
    );

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

    HYPERION_VK_PASS_ERRORS(
        vkFreeDescriptorSets(
            device->GetDevice(),
            m_descriptor_pool,
            uint32_t(m_descriptor_sets_view.size()),
            m_descriptor_sets_view.data()
        ),
        result
    );
    
    // set all to nullptr
    m_descriptor_sets.clear();

    m_descriptor_sets_view.clear();

    /* Destroy pool */
    vkDestroyDescriptorPool(device->GetDevice(), m_descriptor_pool, nullptr);
    m_descriptor_pool = nullptr;

    m_is_created = false;

    return result;
}

DescriptorSet *DescriptorPool::AddDescriptorSet(std::unique_ptr<DescriptorSet> &&descriptor_set)
{
    AssertThrow(descriptor_set != nullptr);

    const uint index = descriptor_set->GetRealIndex();

    if (index >= m_descriptor_sets.size()) {
        if (index == m_descriptor_sets.size()) {
            m_descriptor_sets.emplace_back();
        } else {
            m_descriptor_sets.resize(index + 1);
        }
    }

    m_descriptor_sets[index] = std::move(descriptor_set);

    return m_descriptor_sets[index].get();
}

Result DescriptorPool::CreateDescriptorSet(Device *device, uint index)
{
    AssertThrow(index < m_descriptor_sets.size());

    
    auto *descriptor_set = m_descriptor_sets[index].get();
    AssertThrow(descriptor_set != nullptr);

    const auto desired_index = descriptor_set->GetDesiredIndex();

    DebugLog(
        LogType::Debug,
        "Allocate descriptor set %llu\n",
        index
    );

    auto allocate_result = descriptor_set->Create(device, this);

    if (m_descriptor_sets_view.size() <= desired_index) {
        m_descriptor_sets_view.resize(desired_index + 1);
    }

    m_descriptor_sets_view[desired_index] = descriptor_set->m_set;

    return allocate_result;
}

Result DescriptorPool::Bind(Device *device,
    CommandBuffer *cmd,
    GraphicsPipeline *pipeline,
    const DescriptorSetBinding &binding) const
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

Result DescriptorPool::Bind(Device *device,
    CommandBuffer *cmd,
    ComputePipeline *pipeline,
    const DescriptorSetBinding &binding) const
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

Result DescriptorPool::Bind(Device *device,
    CommandBuffer *cmd,
    RaytracingPipeline *pipeline,
    const DescriptorSetBinding &binding) const
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

    const auto set_index     = DescriptorSet::GetDesiredIndex(binding.declaration.set);
    const auto binding_index = DescriptorSet::GetDesiredIndex(binding.locations.binding);

    AssertThrowMsg(
        binding.declaration.count <= max_bound_descriptor_sets,
        "Requested binding of %d descriptor sets, but maximum bound is %d",
        binding.declaration.count,
        max_bound_descriptor_sets
    );

    AssertThrowMsg(
        set_index < m_descriptor_sets_view.size(),
        "Attempt to bind invalid descriptor set (%u) (at index %u) -- out of bounds (max is %llu)\n",
        static_cast<uint>(binding.declaration.set),
        set_index,
        m_descriptor_sets_view.size()
    );

    auto &bind_set = m_descriptor_sets_view[set_index];

    AssertThrowMsg(
        bind_set != nullptr,
        "Attempt to bind invalid descriptor set %u (at index %u) -- set is null\n",
        static_cast<uint>(binding.declaration.set),
        set_index
    );

    vkCmdBindDescriptorSets(
        cmd->GetCommandBuffer(),
        bind_point,
        pipeline->layout,
        binding_index,
        binding.declaration.count,
        &bind_set,
        static_cast<uint32_t>(binding.offsets.offsets.size()),
        binding.offsets.offsets.data()
    );
}

Result DescriptorPool::CreateDescriptorSetLayout(Device *device, uint index, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out)
{
    if (vkCreateDescriptorSetLayout(device->GetDevice(), layout_create_info, nullptr, out) != VK_SUCCESS) {
        return {Result::RENDERER_ERR, "Could not create descriptor set layout"};
    }
    
    m_descriptor_set_layouts.Insert(index, *out);

    HYPERION_RETURN_OK;
}

Result DescriptorPool::DestroyDescriptorSetLayout(Device *device, uint index)
{
    auto it = m_descriptor_set_layouts.Find(index);

    if (it == m_descriptor_set_layouts.End()) {
        return {Result::RENDERER_ERR, "Could not destroy descriptor set layout; not found in list"};
    }

    vkDestroyDescriptorSetLayout(device->GetDevice(), it->second, nullptr);

    m_descriptor_set_layouts.Erase(it);

    HYPERION_RETURN_OK;
}

VkDescriptorSetLayout DescriptorPool::GetDescriptorSetLayout(uint index)
{
    return m_descriptor_set_layouts.At(index);
}

void DescriptorPool::SetDescriptorSetLayout(uint index, VkDescriptorSetLayout layout)
{
    m_descriptor_set_layouts.Insert(index, layout);
}

Result DescriptorPool::AllocateDescriptorSet(
    Device *device,
    VkDescriptorSetLayout *layout,
    DescriptorSet *out
)
{
    VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.pSetLayouts        = layout;
    alloc_info.descriptorPool     = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;

    constexpr uint32_t max_bindings = DescriptorSet::max_bindless_resources - 1;

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO};
    count_info.descriptorSetCount = 1;
    // This number is the max allocatable count
    count_info.pDescriptorCounts  = &max_bindings;

    if (out->IsBindless()) {
        alloc_info.pNext = &count_info;
    }

    const VkResult alloc_result = vkAllocateDescriptorSets(device->GetDevice(), &alloc_info, &out->m_set);

    switch (alloc_result) {
    case VK_SUCCESS: return Result::OK;
    case VK_ERROR_FRAGMENTED_POOL:
        return {Result::RENDERER_ERR_NEEDS_REALLOCATION, "Fragmented pool", alloc_result};
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return {Result::RENDERER_ERR_NEEDS_REALLOCATION, "Out of pool memory", alloc_result};
    default:
        return {Result::RENDERER_ERR, "Unknown error (check error code)", alloc_result};
    }
}

Descriptor::Descriptor(uint32_t binding, Mode mode)
    : m_binding(binding),
      m_mode(mode),
      m_descriptor_set(nullptr)
{
}

Descriptor::~Descriptor() = default;

void Descriptor::Create(
    Device *device,
    VkDescriptorSetLayoutBinding &binding,
    std::vector<VkWriteDescriptorSet> &writes
)
{
    AssertThrow(m_descriptor_set != nullptr);

    const auto descriptor_type = GetDescriptorType(m_mode);

    m_sub_descriptor_update_indices = {};

    auto descriptor_count = static_cast<uint>(m_sub_descriptors.Size());

    if (m_descriptor_set->IsBindless()) {
        descriptor_count = DescriptorSet::max_bindless_resources;
    } else if (m_descriptor_set->IsTemplate() && m_descriptor_set->GetIndex() == DescriptorSet::Index::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
        descriptor_count = DescriptorSet::max_material_texture_samplers;
    }
    
    binding.descriptorCount    = descriptor_count;
    binding.descriptorType     = descriptor_type;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags         = VK_SHADER_STAGE_ALL;
    binding.binding            = m_binding;

    if (descriptor_count == 0) {
        DebugLog(
            LogType::Debug,
            "Descriptor has 0 subdescriptors, returning from Create() without pushing any writes\n"
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
        
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.pNext           = VK_NULL_HANDLE;
        write.dstBinding      = m_binding;
        write.dstArrayElement = sub_descriptor.element_index;
        write.descriptorCount = 1;
        write.descriptorType  = descriptor_type;
        write.pBufferInfo     = &sub_descriptor.buffer_info;
        write.pImageInfo      = &sub_descriptor.image_info;
        
        if (m_mode == Mode::ACCELERATION_STRUCTURE) {
            write.pNext = &sub_descriptor.acceleration_structure_info;
        }

        writes.push_back(write);
    }
}

void Descriptor::BuildUpdates(Device *, std::vector<VkWriteDescriptorSet> &writes)
{
    const auto descriptor_type = GetDescriptorType(m_mode);

    for (auto &it : m_sub_descriptors) {
        auto &sub_descriptor = it.second;

        UpdateSubDescriptorBuffer(
            sub_descriptor,
            sub_descriptor.buffer_info,
            sub_descriptor.image_info,
            sub_descriptor.acceleration_structure_info
        );
        
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.pNext           = VK_NULL_HANDLE;
        write.dstBinding      = m_binding;
        write.dstArrayElement = sub_descriptor.element_index;
        write.descriptorCount = 1;
        write.descriptorType  = descriptor_type;
        write.pBufferInfo     = &sub_descriptor.buffer_info;
        write.pImageInfo      = &sub_descriptor.image_info;
        
        if (m_mode == Mode::ACCELERATION_STRUCTURE) {
            write.pNext = &sub_descriptor.acceleration_structure_info;
        }

        writes.push_back(write);
    }
    
    /*uint32_t iteration = 0;
        
    while (!m_sub_descriptor_update_indices.empty()) {
        if (iteration == DescriptorSet::max_sub_descriptor_updates_per_frame) {
            break;
        }
        const size_t sub_descriptor_index = m_sub_descriptor_update_indices.front();
        auto &sub_descriptor = m_sub_descriptors.At(sub_descriptor_index);
        UpdateSubDescriptorBuffer(
            sub_descriptor,
            sub_descriptor.buffer_info,
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.pNext           = nullptr;
        write.dstBinding      = m_binding;
        write.dstArrayElement = sub_descriptor.element_index;
        write.descriptorCount = 1;
        write.descriptorType  = descriptor_type;
        write.pBufferInfo     = &sub_descriptor.buffer_info;
        write.pImageInfo      = &sub_descriptor.image_info;
        if (m_mode == Mode::ACCELERATION_STRUCTURE) {
            write.pNext = &sub_descriptor.acceleration_structure_info;
        }
        writes.push_back(write);
        m_dirty_sub_descriptors = m_dirty_sub_descriptors.Excluding(sub_descriptor_index);
        m_sub_descriptor_update_indices.pop_back();
        ++iteration;
    }
    if (m_sub_descriptor_update_indices.empty()) {
        m_dirty_sub_descriptors = {};
    }*/
}

void Descriptor::UpdateSubDescriptorBuffer(
    const SubDescriptor &sub_descriptor,
    VkDescriptorBufferInfo &out_buffer,
    VkDescriptorImageInfo &out_image,
    VkWriteDescriptorSetAccelerationStructureKHR &out_acceleration_structure
) const
{
    switch (m_mode) {
    case Mode::UNIFORM_BUFFER: /* fallthrough */
    case Mode::UNIFORM_BUFFER_DYNAMIC:
    case Mode::STORAGE_BUFFER:
    case Mode::STORAGE_BUFFER_DYNAMIC:
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
    case Mode::IMAGE_SAMPLER:
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
    case Mode::IMAGE_STORAGE:
        AssertThrow(sub_descriptor.image_view != nullptr);
        AssertThrow(sub_descriptor.image_view->GetImageView() != nullptr);

        out_image = {
            .sampler     = VK_NULL_HANDLE,
            .imageView   = sub_descriptor.image_view->GetImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        break;
    case Mode::ACCELERATION_STRUCTURE:
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

uint32_t Descriptor::AddSubDescriptor(SubDescriptor &&sub_descriptor)
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

void Descriptor::RemoveSubDescriptor(uint32_t index)
{
    const auto it = m_sub_descriptors.Find(index);
    AssertThrow(it != m_sub_descriptors.End());
    
    m_sub_descriptors.Erase(it);

    const auto update_it = std::find(
        m_sub_descriptor_update_indices.begin(),
        m_sub_descriptor_update_indices.end(),
        index
    );

    if (update_it != m_sub_descriptor_update_indices.end()) {
        m_sub_descriptor_update_indices.erase(update_it);
    }
}

void Descriptor::MarkDirty(uint32_t sub_descriptor_index)
{
    m_sub_descriptor_update_indices.push_back(sub_descriptor_index);

    m_dirty_sub_descriptors |= {sub_descriptor_index, sub_descriptor_index + 1};

    if (m_descriptor_set != nullptr) {
        m_descriptor_set->m_state = DescriptorSetState::DESCRIPTOR_DIRTY;
    }
}

VkDescriptorType Descriptor::GetDescriptorType(Mode mode)
{
    switch (mode) {
    case Mode::UNIFORM_BUFFER:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case Mode::UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case Mode::STORAGE_BUFFER:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case Mode::STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case Mode::IMAGE_SAMPLER:          return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case Mode::IMAGE_STORAGE:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case Mode::ACCELERATION_STRUCTURE: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type");
    }
}


} // namespace renderer
} // namespace hyperion
