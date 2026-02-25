/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/String.hpp>
#include <Core/utilities/Optional.hpp>
#include <Core/utilities/EnumFlags.hpp>

#include <Core/serialization/fbom/FBOMObject.hpp>
#include <Core/serialization/fbom/FBOMResult.hpp>
#include <Core/serialization/fbom/FBOMType.hpp>
#include <Core/serialization/fbom/FBOMData.hpp>
#include <Core/serialization/fbom/FBOMArray.hpp>
#include <Core/serialization/fbom/FBOMInterfaces.hpp>
#include <Core/serialization/fbom/util/UniqueId.hpp>

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <type_traits>

namespace Hyperion {

enum class FBOMStaticDataFlags : uint32
{
    NONE = 0x0,
    WRITTEN = 0x1
};

HYP_MAKE_ENUM_FLAGS(FBOMStaticDataFlags)

namespace serialization {

struct FBOMStaticData
{
    enum Type
    {
        FBOM_STATIC_DATA_NONE = 0x00,
        FBOM_STATIC_DATA_OBJECT = 0x01,
        FBOM_STATIC_DATA_TYPE = 0x02,
        FBOM_STATIC_DATA_DATA = 0x04,
        FBOM_STATIC_DATA_ARRAY = 0x08,
        FBOM_STATIC_DATA_NAME_TABLE = 0x10
    } type;

    int64 offset;
    UniquePtr<FBOMSerializableBase> data;
    EnumFlags<FBOMStaticDataFlags> flags;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE),
          offset(-1),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMObject& value, int64 offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          data(MakeUnique<FBOMObject>(value)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMType& value, int64 offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          data(MakeUnique<FBOMType>(value)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMData& value, int64 offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data(MakeUnique<FBOMData>(value)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMArray& value, int64 offset = -1)
        : type(FBOM_STATIC_DATA_ARRAY),
          data(MakeUnique<FBOMArray>(value)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMObject&& value, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_OBJECT),
          data(MakeUnique<FBOMObject>(std::move(value))),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMType&& value, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_TYPE),
          data(MakeUnique<FBOMType>(std::move(value))),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMData&& value, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_DATA),
          data(MakeUnique<FBOMData>(std::move(value))),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMArray&& value, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_ARRAY),
          data(MakeUnique<FBOMArray>(std::move(value))),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    FBOMStaticData(const FBOMStaticData& other) = delete;
    FBOMStaticData& operator=(const FBOMStaticData& other) = delete;

    FBOMStaticData(FBOMStaticData&& other) noexcept
        : type(other.type),
          data(std::move(other.data)),
          offset(other.offset),
          flags(other.flags),
          m_id(std::move(other.m_id))
    {
        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = -1;
        other.flags = FBOMStaticDataFlags::NONE;
    }

    FBOMStaticData& operator=(FBOMStaticData&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        type = other.type;
        data = std::move(other.data);
        offset = other.offset;
        flags = other.flags;
        m_id = std::move(other.m_id);

        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = -1;
        other.flags = FBOMStaticDataFlags::NONE;

        return *this;
    }

    ~FBOMStaticData() = default;

    HYP_FORCE_INLINE bool operator<(const FBOMStaticData& other) const
    {
        return offset < other.offset;
    }

    HYP_FORCE_INLINE bool IsWritten() const
    {
        return flags & FBOMStaticDataFlags::WRITTEN;
    }

    HYP_FORCE_INLINE void SetIsWritten(bool isWritten)
    {
        if (isWritten)
        {
            flags |= FBOMStaticDataFlags::WRITTEN;
        }
        else
        {
            flags &= ~FBOMStaticDataFlags::WRITTEN;
        }
    }

    /*! \brief Set a custom identifier for this object (overrides the underlying data's unique identifier) */
    HYP_FORCE_INLINE void SetUniqueID(UniqueId id)
    {
        m_id.Set(id);
    }

    HYP_FORCE_INLINE UniqueId GetUniqueID() const
    {
        if (m_id.HasValue())
        {
            return *m_id;
        }

        if (data != nullptr)
        {
            return data->GetUniqueID();
        }

        return UniqueId::Invalid();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (data != nullptr)
        {
            return data->GetHashCode();
        }

        return HashCode(0);
    }

    HYP_FORCE_INLINE String ToString() const
    {
        if (data != nullptr)
        {
            return data->ToString();
        }

        return "<Unset Data>";
    }

private:
    // Optional custom set Id
    Optional<UniqueId> m_id;
};

} // namespace serialization
} // namespace Hyperion
