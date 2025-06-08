/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_INTERFACES_HPP
#define HYPERION_FBOM_INTERFACES_HPP

#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMEnums.hpp>

#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/String.hpp>

#include <core/Defines.hpp>

#include <HashCode.hpp>

namespace hyperion {
class ByteWriter;

namespace serialization {

class FBOMWriter;
class FBOMReader;
class FBOMLoadContext;

class FBOMSerializableBase
{
public:
    friend class FBOMReader;
    friend class FBOMWriter;

    FBOMSerializableBase() = default;
    FBOMSerializableBase(const FBOMSerializableBase&) = default;
    FBOMSerializableBase& operator=(const FBOMSerializableBase&) = default;
    FBOMSerializableBase(FBOMSerializableBase&&) noexcept = default;
    FBOMSerializableBase& operator=(FBOMSerializableBase&&) noexcept = default;
    virtual ~FBOMSerializableBase() = default;

    virtual FBOMResult Visit(UniqueID id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const = 0;

    virtual UniqueID GetUniqueID() const = 0;
    virtual HashCode GetHashCode() const = 0;
    virtual String ToString(bool deep = true) const = 0;
};

} // namespace serialization
} // namespace hyperion

#endif