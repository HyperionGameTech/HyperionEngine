#pragma once

namespace Hyperion {

enum class DescriptorUsageFlags : uint32
{
    NONE = 0x0,
    DYNAMIC = 0x1
};

HYP_MAKE_ENUM_FLAGS(DescriptorUsageFlags)

struct DescriptorUsage
{
    ShaderRegister slot;
    ShaderInputType type;
    Name setName;
    Name descriptorName;
    ShaderStruct shaderStruct;
    EnumFlags<DescriptorUsageFlags> flags;
    HashMap<String, String> params;

    DescriptorUsage()
        : slot(ShaderRegister::NONE),
          type(ShaderInputType::UNSET),
          setName(Name::Invalid()),
          flags(DescriptorUsageFlags::NONE)
    {
    }

    DescriptorUsage(ShaderRegister slot, ShaderInputType type, Name setName, Name descriptorName, EnumFlags<DescriptorUsageFlags> flags = DescriptorUsageFlags::NONE, HashMap<String, String> params = {})
        : slot(slot),
          type(type),
          setName(setName),
          descriptorName(descriptorName),
          flags(flags),
          params(std::move(params))
    {
    }

    DescriptorUsage(const DescriptorUsage& other)
        : slot(other.slot),
          type(other.type),
          setName(other.setName),
          descriptorName(other.descriptorName),
          shaderStruct(other.shaderStruct),
          flags(other.flags),
          params(other.params)
    {
    }

    DescriptorUsage& operator=(const DescriptorUsage& other)
    {
        if (this == &other)
        {
            return *this;
        }

        slot = other.slot;
        type = other.type;
        setName = other.setName;
        descriptorName = other.descriptorName;
        shaderStruct = other.shaderStruct;
        flags = other.flags;
        params = other.params;

        return *this;
    }

    DescriptorUsage(DescriptorUsage&& other) noexcept
        : slot(other.slot),
          type(other.type),
          setName(std::move(other.setName)),
          descriptorName(std::move(other.descriptorName)),
          shaderStruct(std::move(other.shaderStruct)),
          flags(other.flags),
          params(std::move(other.params))
    {
    }

    DescriptorUsage& operator=(DescriptorUsage&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        slot = other.slot;
        type = other.type;
        setName = std::move(other.setName);
        descriptorName = std::move(other.descriptorName);
        shaderStruct = std::move(other.shaderStruct);
        flags = other.flags;
        params = std::move(other.params);

        return *this;
    }

    ~DescriptorUsage() = default;

    HYP_FORCE_INLINE bool operator==(const DescriptorUsage& other) const
    {
        return slot == other.slot
            && type == other.type
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
        return type == ShaderInputType::UNIFORM_BUFFER
            || type == ShaderInputType::UNIFORM_BUFFER_DYNAMIC
            || type == ShaderInputType::STORAGE_BUFFER
            || type == ShaderInputType::STORAGE_BUFFER_DYNAMIC;
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