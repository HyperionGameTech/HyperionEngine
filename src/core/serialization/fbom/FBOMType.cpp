/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMType.hpp>
#include <core/serialization/fbom/FBOMBaseTypes.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>

#include <core/io/ByteWriter.hpp>

namespace hyperion::serialization {

#pragma region FBOMType

FBOMType::FBOMType()
    : name("UNSET"),
      size(0),
      type_id(TypeId::Void()),
      flags(FBOMTypeFlags::DEFAULT),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView& name, SizeType size, TypeId type_id)
    : name(name),
      size(size),
      type_id(type_id),
      flags(FBOMTypeFlags::DEFAULT),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView& name, SizeType size, TypeId type_id, const FBOMType& extends)
    : name(name),
      size(size),
      type_id(type_id),
      flags(FBOMTypeFlags::DEFAULT),
      extends(new FBOMType(extends))
{
}

FBOMType::FBOMType(const ANSIStringView& name, SizeType size, TypeId type_id, EnumFlags<FBOMTypeFlags> flags)
    : name(name),
      size(size),
      type_id(type_id),
      flags(flags),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView& name, SizeType size, TypeId type_id, EnumFlags<FBOMTypeFlags> flags, const FBOMType& extends)
    : name(name),
      size(size),
      type_id(type_id),
      flags(flags),
      extends(new FBOMType(extends))
{
}

FBOMType::FBOMType(const FBOMType& other)
    : name(other.name),
      size(other.size),
      type_id(other.type_id),
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
    type_id = other.type_id;
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
      type_id(other.type_id),
      flags(other.flags),
      extends(other.extends)
{
    other.size = 0;
    other.type_id = TypeId::Void();
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
    type_id = other.type_id;
    flags = other.flags;
    extends = other.extends;

    other.size = 0;
    other.type_id = TypeId::Void();
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

bool FBOMType::HasAnyFlagsSet(EnumFlags<FBOMTypeFlags> flags, bool include_parents) const
{
    if (this->flags & flags)
    {
        return true;
    }

    if (include_parents && extends)
    {
        return extends->HasAnyFlagsSet(flags, true);
    }

    return false;
}

bool FBOMType::IsOrExtends(const ANSIStringView& name, bool allow_unbounded, bool allow_void_type_id) const
{
    if (this->name == name)
    {
        return true;
    }

    if (extends == nullptr || extends->IsUnset())
    {
        return false;
    }

    return extends->IsOrExtends(name, allow_unbounded, allow_void_type_id);
}

bool FBOMType::Is(const FBOMType& other, bool allow_unbounded, bool allow_void_type_id) const
{
    if (name != other.name)
    {
        return false;
    }

    if (!allow_void_type_id || (type_id && other.type_id))
    {
        if (type_id != other.type_id)
        {
            return false;
        }
    }

    if (!allow_unbounded)
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

        return extends->Is(*other.extends, allow_unbounded, allow_void_type_id);
    }

    return true;
}

bool FBOMType::IsOrExtends(const FBOMType& other, bool allow_unbounded, bool allow_void_type_id) const
{
    if (Is(other, allow_unbounded, allow_void_type_id))
    {
        return true;
    }

    return Extends(other, allow_unbounded, allow_void_type_id);

    // if (*this == other) {
    //     return true;
    // }

    // if (allow_unbounded && (IsUnbounded() || other.IsUnbounded())) {
    //     if (name == other.name && extends == other.extends) { // don't size check
    //         return true;
    //     }
    // }

    // return Extends(other);
}

bool FBOMType::Extends(const FBOMType& other, bool allow_unbounded, bool allow_void_type_id) const
{
    if (extends == nullptr || extends->IsUnset())
    {
        return false;
    }

    if (extends->Is(other, allow_unbounded, allow_void_type_id))
    {
        return true;
    }

    return extends->Extends(other, allow_unbounded, allow_void_type_id);
}

FBOMResult FBOMType::Visit(UniqueID id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const
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

} // namespace hyperion::serialization
