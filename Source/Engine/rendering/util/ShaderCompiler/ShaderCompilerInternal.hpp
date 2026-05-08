#pragma once

namespace Hyperion {

enum class DescriptorUsageFlags : uint32
{
    NONE = 0x0,
    DYNAMIC = 0x1
};

HYP_MAKE_ENUM_FLAGS(DescriptorUsageFlags);

struct DescriptorUsage
{
    ShaderRegister slot = ShaderRegister::NONE;
    ShaderInputType type = ShaderInputType::Unset;
    ShaderResourceCategory category = ShaderResourceCategory::Unknown;
    GpuBufferType bufferType = GpuBufferType::NONE;
    Name setName;
    Name descriptorName;
    ShaderStruct shaderStruct;
    EnumFlags<DescriptorUsageFlags> flags;
    HashMap<String, String> params;

    HYP_FORCE_INLINE bool operator==(const DescriptorUsage& other) const
    {
        return slot == other.slot
            && type == other.type
            && category == other.category
            && bufferType == other.bufferType
            && setName == other.setName
            && descriptorName == other.descriptorName
            && shaderStruct == other.shaderStruct
            && flags == other.flags
            && params == other.params;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsage& other) const
    {
        return slot != other.slot
            || type != other.type
            || category != other.category
            || bufferType != other.bufferType
            || setName != other.setName
            || descriptorName != other.descriptorName
            || shaderStruct != other.shaderStruct
            || flags != other.flags
            || params != other.params;
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsage& other) const
    {
        if (slot != other.slot)
        {
            return slot < other.slot;
        }

        if (type != other.type)
        {
            return type < other.type;
        }

        if (category != other.category)
        {
            return category < other.category;
        }

        if (bufferType != other.bufferType)
        {
            return bufferType < other.bufferType;
        }

        if (setName != other.setName)
        {
            return setName < other.setName;
        }

        if (descriptorName != other.descriptorName)
        {
            return descriptorName < other.descriptorName;
        }

        if (shaderStruct != other.shaderStruct)
        {
            return shaderStruct < other.shaderStruct;
        }

        if (flags != other.flags)
        {
            return uint32(flags) < uint32(other.flags);
        }

        return false;
    }

    /*! \brief Returns true if this is a constant buffer or storage buffer. */
    HYP_FORCE_INLINE bool IsBuffer() const
    {
        return category == ShaderResourceCategory::Buffer;
    }

    HYP_FORCE_INLINE uint32 GetCount() const
    {
        uint32 value = 1;

        auto it = params.Find("count");

        if (it == params.End())
        {
            return value;
        }

        if (StringUtil::Parse(it->second, &value))
        {
            return value;
        }

        return 1;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        if (shaderStruct.HasExplicitSize())
        {
            return shaderStruct.size;
        }

        uint32 value = ~0u;

        auto it = params.Find("size");

        if (it == params.End())
        {
            return value;
        }

        if (StringUtil::Parse(it->second, &value))
        {
            return value;
        }

        return uint32(-1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(slot);
        hc.Add(type);
        hc.Add(category);
        hc.Add(bufferType);
        hc.Add(setName.GetHashCode());
        hc.Add(descriptorName.GetHashCode());
        hc.Add(shaderStruct);
        hc.Add(flags);
        hc.Add(params.GetHashCode());

        return hc;
    }
};

struct DescriptorUsageSet
{
    FlatSet<DescriptorUsage> elements;

    void BuildDescriptorTableDeclaration(ShaderInputGroup& table) const;

    HYP_FORCE_INLINE DescriptorUsage& operator[](size_t index)
    {
        return elements[index];
    }

    HYP_FORCE_INLINE const DescriptorUsage& operator[](size_t index) const
    {
        return elements[index];
    }

    HYP_FORCE_INLINE bool operator==(const DescriptorUsageSet& other) const
    {
        return elements == other.elements;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsageSet& other) const
    {
        return elements != other.elements;
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        return elements.Size();
    }

    HYP_FORCE_INLINE void Add(const DescriptorUsage& descriptorUsage)
    {
        elements.Insert(descriptorUsage);
    }

    HYP_FORCE_INLINE DescriptorUsage* Find(StringHash descriptorName)
    {
        auto it = elements.FindIf([descriptorName](const DescriptorUsage& descriptorUsage)
            {
                return descriptorUsage.descriptorName == descriptorName;
            });

        if (it == elements.End())
        {
            return nullptr;
        }

        return it;
    }

    HYP_FORCE_INLINE const DescriptorUsage* Find(StringHash descriptorName) const
    {
        return const_cast<const DescriptorUsageSet*>(this)->Find(descriptorName);
    }

    HYP_FORCE_INLINE void Merge(const Array<DescriptorUsage>& other)
    {
        elements.Merge(other);
    }

    HYP_FORCE_INLINE void Merge(Array<DescriptorUsage>&& other)
    {
        elements.Merge(std::move(other));
    }

    HYP_FORCE_INLINE void Merge(const DescriptorUsageSet& other)
    {
        elements.Merge(other.elements);
    }

    HYP_FORCE_INLINE void Merge(DescriptorUsageSet&& other)
    {
        elements.Merge(std::move(other.elements));
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return elements.GetHashCode();
    }
};

} // namespace Hyperion
