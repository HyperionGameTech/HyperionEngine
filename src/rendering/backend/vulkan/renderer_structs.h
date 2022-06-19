#ifndef HYPERION_RENDERER_STRUCTS_H
#define HYPERION_RENDERER_STRUCTS_H

#include <util/enum_options.h>
#include <hash_code.h>
#include <types.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <algorithm>
#include <optional>
#include <set>
#include <memory>
#include <utility>
#include <tuple>

namespace hyperion {
namespace renderer {

enum class DatumType {
    UNSIGNED_BYTE,
    SIGNED_BYTE,
    UNSIGNED_SHORT,
    SIGNED_SHORT,
    UNSIGNED_INT,
    SIGNED_INT,
    FLOAT
};

enum class FaceCullMode {
    NONE,
    BACK,
    FRONT
};

enum class FillMode {
    FILL,
    LINE
};

enum class Topology {
    TRIANGLES,
    TRIANGLE_FAN,
    TRIANGLE_STRIP,

    LINES,

    POINTS
};

enum class StencilMode {
    NONE,
    FILL,
    OUTLINE
};

struct StencilState {
    UInt        id   = 0;
    StencilMode mode = StencilMode::NONE;

    HYP_DEF_STRUCT_COMPARE_EQL(StencilState);

    bool operator<(const StencilState &other) const
    {
        return std::tie(id, mode) < std::tie(other.id, other.mode);
    }
};

struct MeshBindingDescription {
    UInt32            binding;
    UInt32            stride;
    VkVertexInputRate input_rate;

    MeshBindingDescription()
        : binding(0), stride(0), input_rate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    MeshBindingDescription(UInt32 binding, UInt32 stride, VkVertexInputRate input_rate)
        : binding(binding), stride(stride), input_rate(input_rate)
    {
    }

    VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bind;
        bind.binding = this->binding;
        bind.stride = this->stride;
        bind.inputRate = this->input_rate;
        return bind;
    }
};

struct VertexAttribute {
    enum Type {
        MESH_INPUT_ATTRIBUTE_UNDEFINED    = 0,
        MESH_INPUT_ATTRIBUTE_POSITION     = 1,
        MESH_INPUT_ATTRIBUTE_NORMAL       = 2,
        MESH_INPUT_ATTRIBUTE_TEXCOORD0    = 4,
        MESH_INPUT_ATTRIBUTE_TEXCOORD1    = 8,
        MESH_INPUT_ATTRIBUTE_TANGENT      = 16,
        MESH_INPUT_ATTRIBUTE_BITANGENT    = 32,
        MESH_INPUT_ATTRIBUTE_BONE_INDICES = 64,
        MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS = 128,
    };

    static const EnumOptions<Type, VertexAttribute, 16> mapping;

    UInt32      location;
    UInt32      binding;
    // total size -- num elements * sizeof(float)
    size_t      size;

    inline bool operator<(const VertexAttribute &other) const
        { return location < other.location; }

    inline VkFormat GetFormat() const
    {
        switch (this->size) {
        case sizeof(float):     return VK_FORMAT_R32_SFLOAT;
        case sizeof(float) * 2: return VK_FORMAT_R32G32_SFLOAT;
        case sizeof(float) * 3: return VK_FORMAT_R32G32B32_SFLOAT;
        case sizeof(float) * 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: AssertThrowMsg(0, "Unsupported vertex attribute format!");
        }
    }

    VkVertexInputAttributeDescription GetAttributeDescription() const {
        VkVertexInputAttributeDescription attrib{};
        attrib.location = this->location;
        attrib.binding = this->binding;
        attrib.format = this->GetFormat();

        return attrib;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(this->location);
        hc.Add(this->binding);
        hc.Add(this->size);

        return hc;
    }
};

struct VertexAttributeSet {
    UInt64 flag_mask;

    constexpr VertexAttributeSet()
        : flag_mask(0) {}
    constexpr VertexAttributeSet(UInt64 flag_mask)
        : flag_mask(flag_mask) {}
    constexpr VertexAttributeSet(VertexAttribute::Type flags)
        : flag_mask(static_cast<UInt64>(flags)) {}
    constexpr VertexAttributeSet(const VertexAttributeSet &other)
        : flag_mask(other.flag_mask) {}

    VertexAttributeSet &operator=(const VertexAttributeSet &other)
    {
        flag_mask = other.flag_mask;

        return *this;
    }

    ~VertexAttributeSet() = default;

    explicit operator bool() const { return bool(flag_mask); }

    bool operator==(const VertexAttributeSet &other) const              { return flag_mask == other.flag_mask; }

    VertexAttributeSet operator~() const                                { return ~flag_mask; }

    VertexAttributeSet operator&(const VertexAttributeSet &other) const { return {flag_mask & other.flag_mask}; }
    VertexAttributeSet &operator&=(const VertexAttributeSet &other)     { flag_mask &= other.flag_mask; return *this; }
    VertexAttributeSet operator&(UInt64 flags) const                  { return {flag_mask & flags}; }
    VertexAttributeSet &operator&=(UInt64 flags)                      { flag_mask &= flags; return *this; }
    
    VertexAttributeSet operator|(const VertexAttributeSet &other) const { return {flag_mask | other.flag_mask}; }
    VertexAttributeSet &operator|=(const VertexAttributeSet &other)     { flag_mask |= other.flag_mask; return *this; }
    VertexAttributeSet operator|(UInt64 flags) const                  { return {flag_mask | flags}; }
    VertexAttributeSet &operator|=(UInt64 flags)                      { flag_mask |= flags; return *this; }

    bool operator<(const VertexAttributeSet &other) const               { return flag_mask < other.flag_mask; }

    bool Has(VertexAttribute::Type type) const { return bool(operator&(UInt64(type))); }

    void Set(UInt64 flags, bool enable = true)
    {
        if (enable) {
            flag_mask |= flags;
        } else {
            flag_mask &= ~flags;
        }
    }

    void Set(VertexAttribute::Type type, bool enable = true)
    {
        Set(UInt64(type), enable);
    }

    void Merge(const VertexAttributeSet &other)
    {
        flag_mask |= other.flag_mask;
    }

    std::vector<VertexAttribute> BuildAttributes() const
    {
        std::vector<VertexAttribute> attributes;
        attributes.reserve(VertexAttribute::mapping.Size());

        for (size_t i = 0; i < VertexAttribute::mapping.Size(); i++) {
            const UInt64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                attributes.push_back(VertexAttribute::mapping[VertexAttribute::Type(iter_flag_mask)]);
            }
        }

        return attributes;
    }

    inline size_t CalculateVertexSize() const
    {
        size_t size = 0;

        for (size_t i = 0; i < VertexAttribute::mapping.Size(); i++) {
            const UInt64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                size += VertexAttribute::mapping[VertexAttribute::Type(iter_flag_mask)].size;
            }
        }

        return size;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flag_mask);

        return hc;
    }
};

constexpr VertexAttributeSet static_mesh_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT
);

constexpr VertexAttributeSet skeleton_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES
);


struct QueueFamilyIndices {
    using Index = UInt32;
    
    std::optional<Index> graphics_family;
    std::optional<Index> transfer_family;
    std::optional<Index> present_family;
    std::optional<Index> compute_family;

    bool IsComplete() const {
        return graphics_family.has_value()
            && transfer_family.has_value()
            && present_family.has_value()
            && compute_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkQueueFamilyProperties> queue_family_properties;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct alignas(8) Extent2D {
    union {
        struct {  // NOLINT(clang-diagnostic-nested-anon-types)
            UInt32 width, height;
        };

        UInt32 v[2];
    };

    Extent2D()
        : width(0),
          height(0)
    {
    }

    Extent2D(UInt32 width, UInt32 height)
        : width(width),
          height(height)
    {
    }

    Extent2D(const Extent2D &other) = default;
    Extent2D &operator=(const Extent2D &other) = default;
    Extent2D(Extent2D &&other) noexcept = default;
    Extent2D &operator=(Extent2D &&other) noexcept = default;
    ~Extent2D() = default;

    bool operator==(const Extent2D &other) const
    {
        return width == other.width
            && height == other.height;
    }

    bool operator!=(const Extent2D &other) const
        { return !operator==(other); }
    
    constexpr UInt32 &operator[](UInt32 index)      { return v[index]; }
    constexpr UInt32 operator[](UInt32 index) const { return v[index]; }

    Vector2 ToVector2() const { return Vector2(static_cast<float>(width), static_cast<float>(height)); }
    UInt32 Size() const { return width * height; }
};

static_assert(sizeof(Extent2D) == 8);

struct alignas(16) Extent3D {
    union {
        struct {  // NOLINT(clang-diagnostic-nested-anon-types)
            UInt32 width, height, depth;
        };

        UInt32 v[3];
    };

    Extent3D()
        : width(0),
          height(0),
          depth(0)
    {
    }

    explicit Extent3D(UInt32 extent)
        : width(extent),
          height(extent),
          depth(extent)
    {
    }

    Extent3D(UInt32 width, UInt32 height, UInt32 depth)
        : width(width),
          height(height),
          depth(depth)
    {
    }

    explicit Extent3D(const Extent2D &extent_2d, UInt32 depth = 1)
        : width(extent_2d.width),
          height(extent_2d.height),
          depth(depth)
    {
    }

    Extent3D(const Extent3D &other) = default;
    Extent3D &operator=(const Extent3D &other) = default;
    Extent3D(Extent3D &&other) noexcept = default;
    Extent3D &operator=(Extent3D &&other) noexcept = default;
    ~Extent3D() = default;

    bool operator==(const Extent3D &other) const
    {
        return width == other.width
            && height == other.height
            && depth == other.depth;
    }

    bool operator!=(const Extent3D &other) const
        { return !operator==(other); }
    
    Extent3D operator*(const Extent3D &other) const { return Extent3D(width * other.width, height * other.height, depth * other.depth); }
    Extent3D &operator*=(const Extent3D &other)
    {
        width *= other.width;
        height *= other.height;
        depth *= other.depth;

        return *this;
    }

    Extent3D operator/(const Extent3D &other) const { return Extent3D(width / other.width, height / other.height, depth / other.depth); }
    Extent3D &operator/=(const Extent3D &other)
    {
        width /= other.width;
        height /= other.height;
        depth /= other.depth;

        return *this;
    }
    
    constexpr UInt32 &operator[](UInt32 index)         { return v[index]; }
    constexpr UInt32 operator[](UInt32 index) const    { return v[index]; }

    Extent2D ToExtent2D() const { return Extent2D(width, height); }
    Vector3 ToVector3() const   { return Vector3(static_cast<float>(width), static_cast<float>(height), static_cast<float>(depth)); }

    UInt32 Size() const { return width * height * depth; }
};

static_assert(sizeof(Extent3D) == 16);

struct PackedVertex {
    float position_x,
          position_y,
          position_z,
          normal_x,
          normal_y,
          normal_z,
          texcoord0_x,
          texcoord0_y;
};

static_assert(sizeof(PackedVertex) % 16 == 0);

struct MeshDescription {
    UInt64 vertex_buffer_address;
    UInt64 index_buffer_address;
};

static_assert(sizeof(MeshDescription) % 16 == 0);

using PackedIndex = UInt32;

/* images */
struct ImageSubResource {
    VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    UInt32 base_array_layer      = 0;
    UInt32 base_mip_level        = 0;
    UInt32 num_layers            = 1;
    UInt32 num_levels            = 1;

    bool operator==(const ImageSubResource &other) const
    {
        return aspect_mask == other.aspect_mask
            && base_array_layer == other.base_array_layer
            && num_layers == other.num_layers
            && base_mip_level == other.base_mip_level
            && num_levels == other.num_levels;
    }
};

template<class ...Args>
class PerFrameData {
    struct FrameDataWrapper {
        std::tuple<std::unique_ptr<Args>...> tup;

        template <class T>
        T *Get()
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        const T *Get() const
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        void Set(std::unique_ptr<T> &&value)
        {
            std::get<std::unique_ptr<T>>(tup) = std::move(value);
        }
    };

public:
    PerFrameData(UInt32 num_frames) : m_num_frames(num_frames)
        { m_data.resize(num_frames); }

    PerFrameData(const PerFrameData &other) = delete;
    PerFrameData &operator=(const PerFrameData &other) = delete;
    PerFrameData(PerFrameData &&) = default;
    PerFrameData &operator=(PerFrameData &&) = default;
    ~PerFrameData() = default;

    inline UInt32 NumFrames() const
        { return m_num_frames; }

    inline FrameDataWrapper &operator[](UInt32 index)
        { return m_data[index]; }

    inline const FrameDataWrapper &operator[](UInt32 index) const
        { return m_data[index]; }

    inline FrameDataWrapper &At(UInt32 index)
        { return m_data[index]; }

    inline const FrameDataWrapper &At(UInt32 index) const
        { return m_data[index]; }

    inline void Reset()
        { m_data = std::vector<FrameDataWrapper>(m_num_frames); }

protected:
    UInt32 m_num_frames;
    std::vector<FrameDataWrapper> m_data;
};

} // namespace renderer
} // namespace hyperion

template <>
struct ::std::hash<hyperion::renderer::ImageSubResource> {
    inline size_t operator()(const hyperion::renderer::ImageSubResource &sub_resource) const
    {
        hyperion::HashCode hc;
        hc.Add(sub_resource.aspect_mask);
        hc.Add(sub_resource.base_array_layer);
        hc.Add(sub_resource.num_layers);
        hc.Add(sub_resource.base_mip_level);
        hc.Add(sub_resource.num_levels);

        return hc.Value();
    }
};

#endif //HYPERION_RENDERER_STRUCTS_H
