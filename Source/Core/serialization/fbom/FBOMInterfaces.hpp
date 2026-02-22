/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMEnums.hpp>
#include <core/serialization/fbom/util/UniqueId.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/String.hpp>

#include <core/Defines.hpp>

#include <core/HashCode.hpp>

namespace Hyperion {
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

    virtual FBOMResult Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const = 0;

    virtual UniqueId GetUniqueID() const = 0;
    virtual HashCode GetHashCode() const = 0;
    virtual String ToString(bool deep = true) const = 0;
};

} // namespace serialization
} // namespace Hyperion
