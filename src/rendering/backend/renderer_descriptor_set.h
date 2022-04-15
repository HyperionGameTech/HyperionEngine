#ifndef HYPERION_RENDERER_DESCRIPTOR_SET_H
#define HYPERION_RENDERER_DESCRIPTOR_SET_H

#include "renderer_result.h"

#include <util/non_owning_ptr.h>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <queue>

namespace hyperion {
namespace renderer {

class Device;
class CommandBuffer;
class GraphicsPipeline;
class ComputePipeline;
class ImageView;
class Sampler;
class GPUBuffer;
class Descriptor;
class DescriptorPool;

enum class DescriptorSetState {
    DESCRIPTOR_CLEAN = 0,
    DESCRIPTOR_DIRTY = 1
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

        DESCRIPTOR_SET_INDEX_MAX
    };

    static constexpr uint32_t max_descriptor_sets = DESCRIPTOR_SET_INDEX_MAX;
    static constexpr uint32_t max_bindless_resources = 1024;
    static constexpr uint32_t max_sub_descriptor_updates_per_frame = 16;
    static constexpr uint32_t max_bound_descriptor_sets = 4; /* 0 = no cap */

    DescriptorSet(bool bindless);
    DescriptorSet(const DescriptorSet &other) = delete;
    DescriptorSet &operator=(const DescriptorSet &other) = delete;
    ~DescriptorSet();

    inline DescriptorSetState GetState() const { return m_state; }
    inline bool IsBindless() const { return m_bindless; }

    template <class DescriptorType, class ...Args>
    Descriptor *AddDescriptor(Args &&... args)
    {
        m_descriptors.push_back(std::make_unique<DescriptorType>(std::move(args)...));
        m_descriptor_bindings.push_back({});

        return m_descriptors.back().get();
    }

    inline Descriptor *GetDescriptor(size_t index) { return m_descriptors[index].get(); }
    inline const Descriptor *GetDescriptor(size_t index) const { return m_descriptors[index].get(); }
    inline std::vector<std::unique_ptr<Descriptor>> &GetDescriptors() { return m_descriptors; }
    inline const std::vector<std::unique_ptr<Descriptor>> &GetDescriptors() const { return m_descriptors; }

    Result Create(Device *device, DescriptorPool *pool);
    Result Destroy(Device *device);

    void ApplyUpdates(Device *device);

    VkDescriptorSet m_set;

private:
    std::vector<std::unique_ptr<Descriptor>> m_descriptors;
    std::vector<VkDescriptorSetLayoutBinding> m_descriptor_bindings; /* one per each descriptor */
    std::vector<VkWriteDescriptorSet> m_descriptor_writes; /* any number of per descriptor - reset after each update */
    DescriptorSetState m_state;
    bool m_bindless;
};

struct DescriptorSetBinding {
    struct Declaration {
        uint32_t set;
        uint32_t count; /* default to num total sets - set */
    } declaration;

    /* where we bind to in the shader program */
    struct Locations {
        uint32_t binding; /* defaults to first_set_index */
    } locations;

    struct DynamicOffsets {
        std::vector<uint32_t> offsets;
    } offsets;

    DescriptorSetBinding()
        : declaration({ .set = 0, .count = DescriptorSet::max_descriptor_sets }),
          locations({ .binding = 0 })
    {
    }

    DescriptorSetBinding(Declaration &&dec)
        : declaration(std::move(dec)),
          locations({ .binding = dec.set })
    {
        if (declaration.count == 0) {
            declaration.count = DescriptorSet::max_descriptor_sets - declaration.set;
        }
    }

    DescriptorSetBinding(Declaration &&dec, Locations &&loc)
        : declaration(std::move(dec)),
          locations(std::move(loc))
    {
        if (declaration.count == 0) {
            declaration.count = DescriptorSet::max_descriptor_sets - declaration.set;
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
    Result CreateDescriptorSetLayout(Device *device, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out);
    Result DestroyDescriptorSetLayout(Device *device, VkDescriptorSetLayout *layout);

public:
    static const std::unordered_map<VkDescriptorType, size_t> items_per_set;

    DescriptorPool();
    DescriptorPool(const DescriptorPool &other) = delete;
    DescriptorPool &operator=(const DescriptorPool &other) = delete;
    ~DescriptorPool();

    inline size_t NumDescriptorSets() const { return m_num_descriptor_sets; }

    inline const std::vector<VkDescriptorSetLayout> &GetDescriptorSetLayouts() const
        { return m_descriptor_set_layouts; }

    // return new descriptor set
    template <class ...Args>
    DescriptorSet &AddDescriptorSet(Args &&... args)
    {
        const size_t index = m_num_descriptor_sets++;

        AssertThrow(index < m_descriptor_sets.size());

        m_descriptor_sets[index] = std::make_unique<DescriptorSet>(std::move(args)...);

        return *m_descriptor_sets[index];
    }

    inline DescriptorSet *GetDescriptorSet(DescriptorSet::Index index) const
        { return m_descriptor_sets[index].get(); }

    Result Create(Device *device);
    Result Destroy(Device *device);
    Result Bind(Device *device, CommandBuffer *cmd, GraphicsPipeline *pipeline, DescriptorSetBinding &&) const;
    Result Bind(Device *device, CommandBuffer *cmd, ComputePipeline *pipeline, DescriptorSetBinding &&) const;

private:
    std::array<std::unique_ptr<DescriptorSet>, DescriptorSet::max_descriptor_sets> m_descriptor_sets;
    size_t m_num_descriptor_sets;
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    VkDescriptorPool m_descriptor_pool;
    std::vector<VkDescriptorSet> m_descriptor_sets_view;
};

class Descriptor {
    friend class DescriptorSet;
public:

    enum class Mode {
        UNSET,
        UNIFORM_BUFFER,
        UNIFORM_BUFFER_DYNAMIC,
        STORAGE_BUFFER,
        STORAGE_BUFFER_DYNAMIC,
        IMAGE_SAMPLER,
        IMAGE_STORAGE
    };

    struct SubDescriptor {
        GPUBuffer *gpu_buffer = nullptr;
        uint32_t range = 0; /* if 0 then it is set to gpu_buffer->size */

        ImageView *image_view = nullptr;
        Sampler *sampler = nullptr;

        bool valid = false; /* set internally to mark objects ready to be popped */
    };

    Descriptor(uint32_t binding, Mode mode);
    Descriptor(const Descriptor &other) = delete;
    Descriptor &operator=(const Descriptor &other) = delete;
    ~Descriptor();

    inline uint32_t GetBinding() const { return m_binding; }
    inline void SetBinding(uint32_t binding) { m_binding = binding; }

    inline DescriptorSetState GetState() const { return m_state; }
    inline void SetState(DescriptorSetState state) { m_state = state; }

    /* Sub descriptor --> ... uniform Thing { ... } things[5]; */
    inline std::vector<SubDescriptor> &GetSubDescriptors()
        { return m_sub_descriptors; }

    /* Sub descriptor --> ... uniform Thing { ... } things[5]; */
    inline const std::vector<SubDescriptor> &GetSubDescriptors() const
        { return m_sub_descriptors; }

    inline SubDescriptor &GetSubDescriptor(size_t index)
        { return m_sub_descriptors[index]; }

    inline const SubDescriptor &GetSubDescriptor(size_t index) const
        { return m_sub_descriptors[index]; }

    /*! \brief Add a sub-descriptor to this descriptor.
     *  Records that a sub-descriptor at the index has been changed,
     *  so you can call this after the descriptor has been initialized.
     * @param sub_descriptor An object containing buffer or image info about the sub descriptor to be added.
     * @returns index of descriptor
     */
    uint32_t AddSubDescriptor(SubDescriptor &&sub_descriptor);
    /*! \brief Remove the sub-descriptor at the given index. */
    void RemoveSubDescriptor(uint32_t index);
    /*! \brief Mark a sub-descriptor as dirty */
    void MarkDirty(uint32_t sub_descriptor_index);

    void Create(Device *device, VkDescriptorSetLayoutBinding &binding, std::vector<VkWriteDescriptorSet> &writes);

protected:
    struct BufferInfo {
        std::vector<VkDescriptorBufferInfo> buffers;
        std::vector<VkDescriptorImageInfo>  images;
    };

    static VkDescriptorType GetDescriptorType(Mode mode);

    void BuildUpdates(Device *device, std::vector<VkWriteDescriptorSet> &writes);
    void UpdateSubDescriptorBuffer(const SubDescriptor &sub_descriptor, VkDescriptorBufferInfo &out_buffer, VkDescriptorImageInfo &out_image) const;

    std::vector<SubDescriptor> m_sub_descriptors;
    std::queue<size_t> m_sub_descriptor_update_indices;

    BufferInfo m_sub_descriptor_buffer;
    DescriptorSetState m_state;

    uint32_t m_binding;
    Mode m_mode;

private:
    DescriptorSet *m_descriptor_set;
};

/* Convenience descriptor classes */

#define HYP_DEFINE_DESCRIPTOR(class_name, mode) \
    class class_name : public Descriptor { \
    public: \
        class_name( \
            uint32_t binding \
        ) : Descriptor( \
            binding, \
            mode) \
        {} \
    }

HYP_DEFINE_DESCRIPTOR(UniformBufferDescriptor,        Mode::UNIFORM_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicUniformBufferDescriptor, Mode::UNIFORM_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(StorageBufferDescriptor,        Mode::STORAGE_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicStorageBufferDescriptor, Mode::STORAGE_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(ImageSamplerDescriptor,         Mode::IMAGE_SAMPLER);
HYP_DEFINE_DESCRIPTOR(ImageStorageDescriptor,         Mode::IMAGE_STORAGE);

#undef HYP_DEFINE_DESCRIPTOR

} // namespace renderer
} // namespace hyperion

#endif