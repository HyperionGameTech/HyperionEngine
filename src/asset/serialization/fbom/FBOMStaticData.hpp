/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_STATIC_DATA_HPP
#define HYPERION_FBOM_STATIC_DATA_HPP

#include <core/containers/TypeMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMNameTable.hpp>

#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {

enum class FBOMStaticDataFlags : uint32
{
    NONE    = 0x0,
    WRITTEN = 0x1
};

HYP_MAKE_ENUM_FLAGS(FBOMStaticDataFlags)

namespace fbom {

using FBOMStaticDataType = Variant<FBOMObject, FBOMType, FBOMData, FBOMNameTable>;

struct FBOMStaticData
{
    enum
    {
        FBOM_STATIC_DATA_NONE = 0x00,
        FBOM_STATIC_DATA_OBJECT = 0x01,
        FBOM_STATIC_DATA_TYPE = 0x02,
        FBOM_STATIC_DATA_DATA = 0x04,
        FBOM_STATIC_DATA_NAME_TABLE = 0x08
    } type;


    int64                           offset;
    FBOMStaticDataType              data;
    EnumFlags<FBOMStaticDataFlags>  flags;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE),
          offset(-1),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMObject &object_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          data(object_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMType &type_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          data(type_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMData &data_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data(data_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMNameTable &name_table_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_NAME_TABLE),
          data(name_table_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMObject &&object, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_OBJECT),
          data(std::move(object)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMType &&type, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_TYPE),
          data(std::move(type)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMData &&data, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_DATA),
          data(std::move(data)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMNameTable &&name_table, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_NAME_TABLE),
          data(std::move(name_table)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    FBOMStaticData(const FBOMStaticData &other)
        : type(other.type),
          data(other.data),
          offset(other.offset),
          flags(other.flags),
          m_id(other.m_id)
    {
    }

    FBOMStaticData &operator=(const FBOMStaticData &other)
    {
        type = other.type;
        data = other.data;
        offset = other.offset;
        flags = other.flags;
        m_id = other.m_id;

        return *this;
    }

    FBOMStaticData(FBOMStaticData &&other) noexcept
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

    FBOMStaticData &operator=(FBOMStaticData &&other) noexcept
    {
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

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const FBOMStaticData &other) const
        { return offset < other.offset; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsWritten() const
        { return flags & FBOMStaticDataFlags::WRITTEN; }

    HYP_FORCE_INLINE
    void SetIsWritten(bool is_written)
    {
        if (is_written) {
            flags |= FBOMStaticDataFlags::WRITTEN;
        } else {
            flags &= ~FBOMStaticDataFlags::WRITTEN;
        }
    }

    /*! \brief Set a custom identifier for this object (overrides the underlying data's unique identifier) */
    HYP_FORCE_INLINE
    void SetUniqueID(UniqueID id)
        { m_id.Set(id); }

    HYP_FORCE_INLINE
    void UnsetCustomUniqueID()
        { m_id.Unset(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    UniqueID GetUniqueID() const
    {
        if (m_id.HasValue()) {
            return *m_id;
        }

        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return data.Get<FBOMObject>().GetUniqueID();
        case FBOM_STATIC_DATA_TYPE:
            return data.Get<FBOMType>().GetUniqueID();
        case FBOM_STATIC_DATA_DATA:
            return data.Get<FBOMData>().GetUniqueID();
        case FBOM_STATIC_DATA_NAME_TABLE:
            return data.Get<FBOMNameTable>().GetUniqueID();
        default:
            return UniqueID(0);
        }
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return data.Get<FBOMObject>().GetHashCode();
        case FBOM_STATIC_DATA_TYPE:
            return data.Get<FBOMType>().GetHashCode();
        case FBOM_STATIC_DATA_DATA:
            return data.Get<FBOMData>().GetHashCode();
        case FBOM_STATIC_DATA_NAME_TABLE:
            return data.Get<FBOMNameTable>().GetHashCode();
        default:
            return HashCode();
        }

        // return hash_code;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    String ToString() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return data.Get<FBOMObject>().ToString();
        case FBOM_STATIC_DATA_TYPE:
            return data.Get<FBOMType>().ToString();
        case FBOM_STATIC_DATA_DATA:
            return data.Get<FBOMData>().ToString();
        case FBOM_STATIC_DATA_NAME_TABLE:
            return data.Get<FBOMNameTable>().ToString();
        default:
            return "???";
        }
    }

private:
    // Optional custom set ID
    Optional<UniqueID>  m_id;
};

} // namespace fbom
} // namespace hyperion

#endif
