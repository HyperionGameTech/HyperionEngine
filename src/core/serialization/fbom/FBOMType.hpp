/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/serialization/fbom/FBOMInterfaces.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/UniqueId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/reflection/TypeId.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <cstddef>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass* GetClass(TypeId);

enum class FBOMTypeFlags : uint8
{
    NONE = 0x0,
    CONTAINER = 0x2,   // uses marshal class to serialize/deserialize after reading the object
    PLACEHOLDER = 0x4, // a placeholder type that is used to represent an unknown type
    NUMERIC = 0x8,     // a numeric type, able to be converted between different numeric types

    DEFAULT = 0x0
};

HYP_MAKE_ENUM_FLAGS(FBOMTypeFlags)

namespace serialization {

class HYP_API FBOMType : public FBOMSerializableBase
{
public:
    ANSIString name;
    SizeType size;
    TypeId typeId;
    EnumFlags<FBOMTypeFlags> flags;
    FBOMType* extends;

    FBOMType();
    FBOMType(const ANSIStringView& name, SizeType size, TypeId typeId);
    FBOMType(const ANSIStringView& name, SizeType size, TypeId typeId, const FBOMType& extends);
    FBOMType(const ANSIStringView& name, SizeType size, TypeId typeId, EnumFlags<FBOMTypeFlags> flags);
    FBOMType(const ANSIStringView& name, SizeType size, TypeId typeId, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends);
    FBOMType(const FBOMType& other);
    FBOMType& operator=(const FBOMType& other);
    FBOMType(FBOMType&& other) noexcept;
    FBOMType& operator=(FBOMType&& other) noexcept;
    virtual ~FBOMType() override;

    FBOMType Extend(const FBOMType& object) const;

    bool HasAnyFlagsSet(EnumFlags<FBOMTypeFlags> flags, bool includeParents = true) const;

    bool IsType(const FBOMType& other, bool allowUnbounded = true, bool allowVoidTypeId = true) const;
    bool IsOrExtends(const ANSIStringView& name, bool allowUnbounded = true, bool allowVoidTypeId = true) const;
    bool IsOrExtends(const FBOMType& other, bool allowUnbounded = true, bool allowVoidTypeId = true) const;
    bool Extends(const FBOMType& other, bool allowUnbounded = true, bool allowVoidTypeId = true) const;

    HYP_FORCE_INLINE bool IsUnbounded() const
    {
        return size == SizeType(-1);
    }

    HYP_FORCE_INLINE bool IsUnset() const
    {
        return name == "UNSET";
    }

    HYP_FORCE_INLINE bool IsPlaceholder() const
    {
        return HasAnyFlagsSet(FBOMTypeFlags::PLACEHOLDER, false);
    }

    HYP_FORCE_INLINE bool UsesMarshal() const
    {
        return HasAnyFlagsSet(FBOMTypeFlags::CONTAINER, false);
    }

    HYP_FORCE_INLINE bool IsNumeric() const
    {
        return HasAnyFlagsSet(FBOMTypeFlags::NUMERIC, false);
    }

    HYP_FORCE_INLINE bool operator==(const FBOMType& other) const
    {
        return name == other.name
            && size == other.size
            && typeId == other.typeId
            && flags == other.flags
            && extends == other.extends;
    }

    HYP_FORCE_INLINE bool operator!=(const FBOMType& other) const
    {
        return name != other.name
            || size != other.size
            || typeId != other.typeId
            || flags != other.flags
            || extends != other.extends;
    }

    /*! \brief Get the C++ TypeId of this type object.
     *  \note Not all types will give a valid TypeId, which is OK - not all types correspond
     *  directly to a C++ type. */
    HYP_FORCE_INLINE TypeId GetNativeTypeId() const
    {
        return typeId;
    }

    HYP_FORCE_INLINE bool HasNativeTypeId() const
    {
        return typeId != TypeId::Void();
    }

    /*! \brief Gets a pointer to the HypClass that corresponds to the native TypeId for this type.
     *  If there is no valid TypeId for this object, or the native type corresponding to the native TypeId for this object
     *  does not have a corresponding HypClass, nullptr will be returned. */
    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        if (!typeId)
        {
            return nullptr;
        }

        return GetClass(typeId);
    }

    FBOMResult Visit(FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
    {
        return Visit(GetUniqueID(), writer, out, attributes);
    }

    virtual FBOMResult Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;

    virtual UniqueId GetUniqueID() const override
    {
        return UniqueId(GetHashCode());
    }

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;

        hc.Add(name);
        hc.Add(size);
        hc.Add(typeId);

        if (extends != nullptr)
        {
            hc.Add(extends->GetHashCode());
        }

        return hc;
    }
};

} // namespace serialization
} // namespace hyperion
