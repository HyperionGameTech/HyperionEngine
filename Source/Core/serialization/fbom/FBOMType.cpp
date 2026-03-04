/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/serialization/fbom/FBOMType.hpp>
#include <Core/serialization/fbom/FBOMBaseTypes.hpp>
#include <Core/serialization/fbom/FBOMWriter.hpp>

#include <Core/io/ByteWriter.hpp>

namespace Hyperion::serialization {

#pragma region FBOMType

FBOMType::FBOMType()
    : name("UNSET"),
      size(0),
      typeId(TypeId::Void()),
      flags(FBOMTypeFlags::DEFAULT),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView& name, size_t size, TypeId typeId)
    : name(name),
      size(size),
      typeId(typeId),
      flags(FBOMTypeFlags::DEFAULT),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView& name, size_t size, TypeId typeId, const FBOMType& extends)
    : name(name),
      size(size),
      typeId(typeId),
      flags(FBOMTypeFlags::DEFAULT),
      extends(new FBOMType(extends))
{
}

FBOMType::FBOMType(const ANSIStringView& name, size_t size, TypeId typeId, EnumFlags<FBOMTypeFlags> flags)
    : name(name),
      size(size),
      typeId(typeId),
      flags(flags),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView& name, size_t size, TypeId typeId, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
    : name(name),
      size(size),
      typeId(typeId),
      flags(flags),
      extends(new FBOMType(extends))
{
}

FBOMType::FBOMType(const FBOMType& other)
    : name(other.name),
      size(other.size),
      typeId(other.typeId),
      flags(other.flags),
      extends(nullptr)
{
    if (other.extends != nullptr)
    {
        extends = new FBOMType(*other.extends);
    }
}

FBOMType& FBOMType::operator=(const FBOMType& other)
{
    if (extends != nullptr)
    {
        delete extends;
    }

    name = other.name;
    size = other.size;
    typeId = other.typeId;
    flags = other.flags;
    extends = nullptr;

    if (other.extends != nullptr)
    {
        extends = new FBOMType(*other.extends);
    }

    return *this;
}

FBOMType::FBOMType(FBOMType&& other) noexcept
    : name(std::move(other.name)),
      size(other.size),
      typeId(other.typeId),
      flags(other.flags),
      extends(other.extends)
{
    other.size = 0;
    other.typeId = TypeId::Void();
    other.flags = FBOMTypeFlags::DEFAULT;
    other.extends = nullptr;
}

FBOMType& FBOMType::operator=(FBOMType&& other) noexcept
{
    if (extends != nullptr)
    {
        delete extends;
    }

    name = std::move(other.name);
    size = other.size;
    typeId = other.typeId;
    flags = other.flags;
    extends = other.extends;

    other.size = 0;
    other.typeId = TypeId::Void();
    other.flags = FBOMTypeFlags::DEFAULT;
    other.extends = nullptr;

    return *this;
}

FBOMType::~FBOMType()
{
    if (extends != nullptr)
    {
        delete extends;
    }
}

FBOMType FBOMType::Extend(const FBOMType& object) const
{
    return FBOMType(object.name, -1, TypeId::Void(), object.flags, *this);
}

bool FBOMType::HasAnyFlagsSet(EnumFlags<FBOMTypeFlags> flags, bool includeParents) const
{
    if (this->flags & flags)
    {
        return true;
    }

    if (includeParents && extends)
    {
        return extends->HasAnyFlagsSet(flags, true);
    }

    return false;
}

bool FBOMType::IsOrExtends(const ANSIStringView& name, bool allowUnbounded, bool allowVoidTypeId) const
{
    if (this->name == name)
    {
        return true;
    }

    if (extends == nullptr || extends->IsUnset())
    {
        return false;
    }

    return extends->IsOrExtends(name, allowUnbounded, allowVoidTypeId);
}

bool FBOMType::IsType(const FBOMType& other, bool allowUnbounded, bool allowVoidTypeId) const
{
    if (name != other.name)
    {
        return false;
    }

    if (!allowVoidTypeId || (typeId && other.typeId))
    {
        if (typeId != other.typeId)
        {
            return false;
        }
    }

    if (!allowUnbounded)
    {
        if (size != other.size)
        {
            return false;
        }
    }

    if (extends != nullptr)
    {
        if (!other.extends)
        {
            return false;
        }

        return extends->IsType(*other.extends, allowUnbounded, allowVoidTypeId);
    }

    return true;
}

bool FBOMType::IsOrExtends(const FBOMType& other, bool allowUnbounded, bool allowVoidTypeId) const
{
    if (IsType(other, allowUnbounded, allowVoidTypeId))
    {
        return true;
    }

    return Extends(other, allowUnbounded, allowVoidTypeId);

    // if (*this == other) {
    //     return true;
    // }

    // if (allowUnbounded && (IsUnbounded() || other.IsUnbounded())) {
    //     if (name == other.name && extends == other.extends) { // don't size check
    //         return true;
    //     }
    // }

    // return Extends(other);
}

bool FBOMType::Extends(const FBOMType& other, bool allowUnbounded, bool allowVoidTypeId) const
{
    if (extends == nullptr || extends->IsUnset())
    {
        return false;
    }

    if (extends->IsType(other, allowUnbounded, allowVoidTypeId))
    {
        return true;
    }

    return extends->Extends(other, allowUnbounded, allowVoidTypeId);
}

FBOMResult FBOMType::Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const
{
    return writer->Write(out, *this, id, attributes);
}

String FBOMType::ToString(bool deep) const
{
    String str = String(name) + " (" + String::ToString(size) + ") ";

    if (extends != nullptr && !extends->IsUnset())
    {
        str += "[" + extends->ToString(deep) + "]";
    }

    return str;
}

#pragma endregion FBOMType

} // namespace Hyperion::serialization
