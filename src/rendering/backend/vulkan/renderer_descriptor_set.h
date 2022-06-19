#ifndef HYPERION_RENDERER_DESCRIPTOR_SET_H
#define HYPERION_RENDERER_DESCRIPTOR_SET_H

#include <rendering/backend/renderer_result.h>

#include <util/defines.h>
#include <constants.h>

#include <core/lib/range.h>
#include <core/lib/flat_set.h>
#include <core/lib/flat_map.h>

#include <types.h>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <map>
#include <deque>
#include <type_traits>

namespace hyperion {
namespace renderer {

class Device;
class CommandBuffer;
class Pipeline;
class GraphicsPipeline;
class ComputePipeline;
class RaytracingPipeline;
class ImageView;
class Sampler;
class GPUBuffer;
class AccelerationStructure;

class DescriptorSet;
class DescriptorPool;

enum class DescriptorSetState {
    DESCRIPTOR_CLEAN = 0,
    DESCRIPTOR_DIRTY = 1
};

enum class DescriptorType {
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    SAMPLER,
    IMAGE_SAMPLER,
    IMAGE_STORAGE,
    ACCELERATION_STRUCTURE
};

class Descriptor {
    friend class DescriptorSet;
public:

    struct SubDescriptor {
        UInt element_index = ~0u; /* ~0 == just use index of item added */

        union {
            struct /* BufferData */ {
                const GPUBuffer *buffer;
                UInt range; /* if 0 then it is set to buffer->size */

                VkDescriptorBufferInfo buffer_info;
            };

            struct /* ImageData */ {
                const ImageView *image_view;
                const Sampler *sampler;

                VkDescriptorImageInfo image_info;
            };

            struct /* AccelerationStructureData */ {
                const AccelerationStructure *acceleration_structure;

                VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info;
            };

            struct {
                uint64_t raw;
            };
        };

        bool valid = false; /* set internally to mark objects ready to be popped */

        HYP_DEF_STRUCT_COMPARE_EQL(SubDescriptor);

        bool operator<(const SubDescriptor &other) const
        {
            return std::tie(element_index, raw, valid) < std::tie(other.element_index, other.raw, other.valid);
        }
    };

    Descriptor(UInt binding, DescriptorType descriptor_type);
    Descriptor(const Descriptor &other) = delete;
    Descriptor &operator=(const Descriptor &other) = delete;
    ~Descriptor();

    UInt GetBinding() const               { return m_binding; }
    void SetBinding(UInt binding)         { m_binding = binding; }
                                          
    /* Sub descriptor --> ... uniform Thing { ... } things[5]; */
    auto &GetSubDescriptors()             { return m_sub_descriptors; }

    /* Sub descriptor --> ... uniform Thing { ... } things[5]; */
    const auto &GetSubDescriptors() const { return m_sub_descriptors; }

    SubDescriptor &GetSubDescriptor(UInt index)
        { return m_sub_descriptors.At(index); }

    const SubDescriptor &GetSubDescriptor(UInt index) const
        { return m_sub_descriptors.At(index); }

    /*! \brief Add a sub-descriptor to this descriptor.
     *  Records that a sub-descriptor at the index has been changed,
     *  so you can call this after the descriptor has been initialized.
     * @param sub_descriptor An object containing buffer or image info about the sub descriptor to be added.
     * @returns index of descriptor
     */
    UInt SetSubDescriptor(SubDescriptor &&sub_descriptor);
    /*! \brief Remove the sub-descriptor at the given index. */
    void RemoveSubDescriptor(UInt index);
    /*! \brief Mark a sub-descriptor as dirty */
    void MarkDirty(UInt sub_descriptor_index);

    void Create(
        Device *device,
        VkDescriptorSetLayoutBinding &binding,
        std::vector<VkWriteDescriptorSet> &writes
    );

protected:

    static VkDescriptorType ToVkDescriptorType(DescriptorType descriptor_type);

    void BuildUpdates(Device *device, std::vector<VkWriteDescriptorSet> &writes);
    void UpdateSubDescriptorBuffer(const SubDescriptor &sub_descriptor,
        VkDescriptorBufferInfo &out_buffer,
        VkDescriptorImageInfo &out_image,
        VkWriteDescriptorSetAccelerationStructureKHR &out_acceleration_structure) const;

    Range<UInt> m_dirty_sub_descriptors;

    FlatMap<UInt, SubDescriptor> m_sub_descriptors;
    std::deque<size_t> m_sub_descriptor_update_indices;

    UInt m_binding;
    DescriptorType m_descriptor_type;

private:
    DescriptorSet *m_descriptor_set;
};

enum class DescriptorKey {
    UNUSED = 0,

    GBUFFER_TEXTURES,
    GBUFFER_DEPTH,
    DEFERRED_RESULT,
    POST_FX_PRE_STACK,
    POST_FX_POST_STACK,
    POST_FX_UNIFORMS,

    SCENE_BUFFER,
    LIGHTS_BUFFER,
    SHADOW_MAPS,
    SHADOW_MATRICES,

    MATERIAL_BUFFER,
    OBJECT_BUFFER,
    SKELETON_BUFFER,

    SAMPLER,
    TEXTURES

    // ... 

};

class DescriptorSet {
    friend class Descriptor;
public:
    enum Index {
        DESCRIPTOR_SET_INDEX_UNUSED,         /* unused at the moment, pending removal */

        DESCRIPTOR_SET_INDEX_GLOBAL,         /* global, ideally bound once at beginning of frame */
        DESCRIPTOR_SET_INDEX_SCENE,          /* bound per scene / pass */
        DESCRIPTOR_SET_INDEX_OBJECT,         /* bound per each object */

        DESCRIPTOR_SET_INDEX_SCENE_FRAME_1,  /* per scene - frame #2 (frames in flight) */
        DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1, /* per object - frame #2 (frames in flight) */

        DESCRIPTOR_SET_INDEX_BINDLESS,
        DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1,

        DESCRIPTOR_SET_INDEX_VOXELIZER,

        DESCRIPTOR_SET_INDEX_RAYTRACING,

        DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES,

        DESCRIPTOR_SET_INDEX_MAX
    };
    
    static constexpr Index scene_buffer_mapping[]  = {
        DESCRIPTOR_SET_INDEX_SCENE,
        DESCRIPTOR_SET_INDEX_SCENE_FRAME_1
    };

    static constexpr Index object_buffer_mapping[] = {
        DESCRIPTOR_SET_INDEX_OBJECT,
        DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1
    };

    static constexpr Index bindless_textures_mapping[] = {
        DESCRIPTOR_SET_INDEX_BINDLESS,
        DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1
    };

    static constexpr UInt max_descriptor_sets                  = 5000;
    static constexpr UInt max_bindless_resources               = 4096;
    static constexpr UInt max_sub_descriptor_updates_per_frame = 16;
    static constexpr UInt max_bound_descriptor_sets            = 4; /* 0 = no cap */
    static constexpr UInt max_material_texture_samplers        = 16;

    static const std::map<Index, std::map<DescriptorKey, UInt>> mappings;
    static const std::unordered_map<Index, UInt> desired_indices;
    static Index GetBaseIndex(UInt index); // map index to the real index used (this is per-frame stuff)
    static Index GetPerFrameIndex(Index index, UInt frame_index);
    static Index GetPerFrameIndex(Index index, UInt instance_index, UInt frame_index);
    /*! Get the per-frame index of a descriptor set's /real/ index. Returns -1 if applicable to any. */
    static int GetFrameIndex(UInt real_index);
    static UInt GetDesiredIndex(Index index);

    DescriptorSet(Index index, UInt real_index, bool bindless);
    DescriptorSet(const DescriptorSet &other) = delete;
    DescriptorSet &operator=(const DescriptorSet &other) = delete;
    ~DescriptorSet();

    DescriptorSetState GetState() const { return m_state; }
    Index GetIndex() const              { return m_index; }
    UInt GetRealIndex() const           { return m_real_index; }
    bool IsBindless() const             { return m_bindless; }
    UInt GetDesiredIndex() const        { return GetDesiredIndex(DescriptorSet::Index(GetRealIndex())); }

    /* doesn't allocate a descriptor set, just a template for other material textures to follow. Creates a layout. */
    bool IsTemplate() const             { return GetRealIndex() == static_cast<UInt>(Index::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES); }

    template <class DescriptorType>
    Descriptor *AddDescriptor(DescriptorKey key)
    {
        return AddDescriptor<DescriptorType>(DescriptorKeyToIndex(key));
    }

    template <class DescriptorType>
    Descriptor *AddDescriptor(UInt binding)
    {
        static_assert(std::is_base_of_v<Descriptor, DescriptorType>, "DescriptorType must be a derived class of Descriptor");

        m_descriptors.push_back(std::make_unique<DescriptorType>(binding));
        m_descriptor_bindings.push_back({});

        return m_descriptors.back().get();
    }

    bool RemoveDescriptor(Descriptor *descriptor);
    bool RemoveDescriptor(DescriptorKey key);
    bool RemoveDescriptor(UInt binding);

    Descriptor *GetDescriptor(DescriptorKey key) const;
    Descriptor *GetDescriptor(UInt binding) const;

    template <class DescriptorType>
    Descriptor *GetOrAddDescriptor(DescriptorKey key)
    {
        return GetOrAddDescriptor<DescriptorType>(DescriptorKeyToIndex(key));
    }

    template <class DescriptorType>
    Descriptor *GetOrAddDescriptor(UInt binding)
    {
        static_assert(std::is_base_of_v<Descriptor, DescriptorType>, "DescriptorType must be a derived class of Descriptor");

        if (auto *descriptor = GetDescriptor(binding)) {
            return descriptor;
        }

        return AddDescriptor<DescriptorType>(binding);
    }

    std::vector<std::unique_ptr<Descriptor>> &GetDescriptors()             { return m_descriptors; }
    const std::vector<std::unique_ptr<Descriptor>> &GetDescriptors() const { return m_descriptors; }

    Result Create(Device *device, DescriptorPool *pool);
    Result Destroy(Device *device);

    void ApplyUpdates(Device *device);

    VkDescriptorSet m_set;

private:
    UInt DescriptorKeyToIndex(DescriptorKey key) const;

    DescriptorPool *m_descriptor_pool;
    std::vector<std::unique_ptr<Descriptor>> m_descriptors;
    std::vector<VkDescriptorSetLayoutBinding> m_descriptor_bindings; /* one per each descriptor */
    std::vector<VkWriteDescriptorSet> m_descriptor_writes; /* any number of per descriptor - reset after each update */
    DescriptorSetState m_state;
    Index m_index;
    UInt m_real_index;
    bool m_bindless;
};

struct DescriptorSetBinding {
    struct Declaration {
        DescriptorSet::Index set;
        UInt count = 1;
    } declaration;

    /* where we bind to in the shader program */
    struct Locations {
        DescriptorSet::Index binding; /* defaults to `set` */
    } locations;

    struct DynamicOffsets {
        std::vector<UInt> offsets;
    } offsets;

    DescriptorSetBinding()
        : declaration({
              .set = DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED,
              .count = DescriptorSet::max_descriptor_sets
          }),
          locations({
              .binding = DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED
          })
    {
    }

    DescriptorSetBinding(Declaration &&dec)
        : declaration(std::move(dec)),
          locations({})
    {
        locations.binding = declaration.set;

        if (declaration.count == 0) {
            declaration.count = DescriptorSet::DESCRIPTOR_SET_INDEX_MAX - declaration.set;
        }
    }

    DescriptorSetBinding(Declaration &&dec, Locations &&loc)
        : declaration(std::move(dec)),
          locations(std::move(loc))
    {
        if (declaration.count == 0) {
            declaration.count = DescriptorSet::DESCRIPTOR_SET_INDEX_MAX - declaration.set;
        }
    }

    DescriptorSetBinding(Declaration &&dec, Locations &&loc, DynamicOffsets &&offsets)
        : DescriptorSetBinding(std::move(dec), std::move(loc))
    {
        this->offsets = std::move(offsets);  // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }
};

class DescriptorPool {
    friend class DescriptorSet;

    Result AllocateDescriptorSet(Device *device, VkDescriptorSetLayout *layout, DescriptorSet *out);
    Result CreateDescriptorSetLayout(Device *device, UInt index, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out);
    Result DestroyDescriptorSetLayout(Device *device, UInt index);
    VkDescriptorSetLayout GetDescriptorSetLayout(UInt index);

public:
    static const std::unordered_map<VkDescriptorType, size_t> items_per_set;

    DescriptorPool();
    DescriptorPool(const DescriptorPool &other) = delete;
    DescriptorPool &operator=(const DescriptorPool &other) = delete;
    ~DescriptorPool();
    
    VkDescriptorPool GetHandle() const          { return m_descriptor_pool; }

    size_t NumDescriptorSets() const            { return m_descriptor_sets.size(); }
    bool IsCreated() const                      { return m_is_created; }

    auto &GetDescriptorSetLayouts()             { return m_descriptor_set_layouts; }
    const auto &GetDescriptorSetLayouts() const { return m_descriptor_set_layouts; }

    void SetDescriptorSetLayout(UInt index, VkDescriptorSetLayout layout);

    // return new descriptor set
    DescriptorSet *AddDescriptorSet(std::unique_ptr<DescriptorSet> &&descriptor_set);
    DescriptorSet *GetDescriptorSet(DescriptorSet::Index index) const
        { return m_descriptor_sets[index].get(); }

    void RemoveDescriptorSet(DescriptorSet *descriptor_set);
    void RemoveDescriptorSet(UInt index);

    Result Create(Device *device);
    Result Destroy(Device *device);
    Result Bind(Device *device, CommandBuffer *cmd, GraphicsPipeline *pipeline, const DescriptorSetBinding &) const;
    Result Bind(Device *device, CommandBuffer *cmd, ComputePipeline *pipeline, const DescriptorSetBinding &) const;
    Result Bind(Device *device, CommandBuffer *cmd, RaytracingPipeline *pipeline, const DescriptorSetBinding &) const;

    Result CreateDescriptorSet(Device *device, UInt index);
    Result DestroyDescriptorSet(Device *device, UInt index);
    Result DestroyPendingDescriptorSets(Device *device, UInt frame_index);
    Result UpdateDescriptorSets(Device *device, UInt frame_index);

private:
    void BindDescriptorSets(
        Device *device,
        CommandBuffer *cmd,
        VkPipelineBindPoint bind_point,
        Pipeline *pipeline,
        const DescriptorSetBinding &binding
    ) const;

    std::vector<std::unique_ptr<DescriptorSet>> m_descriptor_sets;
    FlatMap<UInt, VkDescriptorSetLayout> m_descriptor_set_layouts;
    VkDescriptorPool m_descriptor_pool;
    std::vector<VkDescriptorSet> m_descriptor_sets_view;

    // 1 for each frame in flight
    std::array<std::deque<UInt>, max_frames_in_flight> m_descriptor_sets_pending_destruction;

    bool m_is_created;
};

/* Convenience descriptor classes */

#define HYP_DEFINE_DESCRIPTOR(class_name, descriptor_type) \
    class class_name : public Descriptor { \
    public: \
        class_name( \
            UInt binding \
        ) : Descriptor(binding, descriptor_type) {} \
    }

HYP_DEFINE_DESCRIPTOR(UniformBufferDescriptor,        DescriptorType::UNIFORM_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicUniformBufferDescriptor, DescriptorType::UNIFORM_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(StorageBufferDescriptor,        DescriptorType::STORAGE_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicStorageBufferDescriptor, DescriptorType::STORAGE_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(ImageDescriptor,                DescriptorType::IMAGE);
HYP_DEFINE_DESCRIPTOR(SamplerDescriptor,              DescriptorType::SAMPLER);
HYP_DEFINE_DESCRIPTOR(ImageSamplerDescriptor,         DescriptorType::IMAGE_SAMPLER);
HYP_DEFINE_DESCRIPTOR(StorageImageDescriptor,         DescriptorType::IMAGE_STORAGE);
HYP_DEFINE_DESCRIPTOR(TlasDescriptor,                 DescriptorType::ACCELERATION_STRUCTURE);

#undef HYP_DEFINE_DESCRIPTOR

} // namespace renderer
} // namespace hyperion

#endif
