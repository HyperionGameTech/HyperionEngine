/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_TYPE_HPP
#define HYPERION_FBOM_TYPE_HPP

#include <asset/serialization/fbom/FBOMInterfaces.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/TypeID.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <cstddef>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass *GetClass(TypeID);

enum class FBOMTypeFlags : uint8
{
    NONE        = 0x0,
    CONTAINER   = 0x2, // uses marshal class to serialize/deserialize after reading the object
    PLACEHOLDER = 0x4, // a placeholder type that is used to represent an unknown type

    DEFAULT     = 0x0
};

HYP_MAKE_ENUM_FLAGS(FBOMTypeFlags)

namespace fbom {

class HYP_API FBOMType : public IFBOMSerializable
{
public:
    ANSIString                  name;
    SizeType                    size;
    TypeID                      type_id;
    EnumFlags<FBOMTypeFlags>    flags;
    FBOMType                    *extends;

    FBOMType();
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id);
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id, const FBOMType &extends);
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id, EnumFlags<FBOMTypeFlags> flags);
    FBOMType(const ANSIStringView &name, SizeType size, TypeID type_id, EnumFlags<FBOMTypeFlags> flags, const FBOMType &extends);
    FBOMType(const FBOMType &other);
    FBOMType &operator=(const FBOMType &other);
    FBOMType(FBOMType &&other) noexcept;
    FBOMType &operator=(FBOMType &&other) noexcept;
    virtual ~FBOMType() override;

    FBOMType Extend(const FBOMType &object) const;

    bool HasAnyFlagsSet(EnumFlags<FBOMTypeFlags> flags, bool include_parents = true) const;

    bool Is(const FBOMType &other, bool allow_unbounded = true, bool allow_void_type_id = true) const;
    bool IsOrExtends(const ANSIStringView &name, bool allow_unbounded = true, bool allow_void_type_id = true) const;
    bool IsOrExtends(const FBOMType &other, bool allow_unbounded = true, bool allow_void_type_id = true) const;
    bool Extends(const FBOMType &other, bool allow_unbounded = true, bool allow_void_type_id = true) const;

    HYP_FORCE_INLINE bool IsUnbounded() const
        { return size == SizeType(-1); }

    HYP_FORCE_INLINE bool IsUnset() const
        { return name == "UNSET"; }

    HYP_FORCE_INLINE bool IsPlaceholder() const
        { return HasAnyFlagsSet(FBOMTypeFlags::PLACEHOLDER, false); }

    HYP_FORCE_INLINE bool UsesMarshal() const
        { return HasAnyFlagsSet(FBOMTypeFlags::CONTAINER, false); }

    HYP_FORCE_INLINE bool operator==(const FBOMType &other) const
    {
        return name == other.name
            && size == other.size
            && type_id == other.type_id
            && flags == other.flags
            && extends == other.extends;
    }

    HYP_FORCE_INLINE bool operator!=(const FBOMType &other) const
    {
        return name != other.name
            || size != other.size
            || type_id != other.type_id
            || flags != other.flags
            || extends != other.extends;
    }

    /*! \brief Get the C++ TypeID of this type object.
     *  \note Not all types will give a valid TypeID, which is OK - not all types correspond
     *  directly to a C++ type. */
    HYP_FORCE_INLINE TypeID GetNativeTypeID() const
        { return type_id; }

    HYP_FORCE_INLINE bool HasNativeTypeID() const
        { return type_id != TypeID::Void(); }

    /*! \brief Gets a pointer to the HypClass that corresponds to the native TypeID for this type.
     *  If there is no valid TypeID for this object, or the native type corresponding to the native TypeID for this object
     *  does not have a corresponding HypClass, nullptr will be returned. */
    HYP_FORCE_INLINE const HypClass *GetHypClass() const
    {
        if (!type_id) {
            return nullptr;
        }

        return GetClass(type_id);
    }

    FBOMResult Visit(FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
        { return Visit(GetUniqueID(), writer, out, attributes); }

    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;

    virtual UniqueID GetUniqueID() const override
        { return UniqueID(GetHashCode()); }
    
    virtual HashCode GetHashCode() const override
    {
        HashCode hc;

        hc.Add(name);
        hc.Add(size);
        hc.Add(type_id);

        if (extends != nullptr) {
            hc.Add(extends->GetHashCode());
        }

        return hc;
    }
};

} // namespace fbom
} // namespace hyperion

#endif
