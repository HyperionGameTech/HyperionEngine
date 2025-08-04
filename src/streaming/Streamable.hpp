/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>
#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/object/HypObject.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT()
struct StreamableKey
{
    HYP_FIELD(Property = "UUID", Serialize = true)
    UUID uuid = UUID::Invalid();

    HYP_FIELD(Property = "AssetPath", Serialize = true)
    Name assetPath;

    HYP_FORCE_INLINE bool operator==(const StreamableKey& other) const
    {
        return uuid == other.uuid
            && assetPath == other.assetPath;
    }

    HYP_FORCE_INLINE bool operator!=(const StreamableKey& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(uuid)
            .Combine(HashCode::GetHashCode(assetPath));
    }
};

HYP_CLASS(Abstract)
class HYP_API StreamableBase : public HypObjectBase
{
    HYP_OBJECT_BODY(StreamableBase);

public:
    StreamableBase() = default;
    StreamableBase(const StreamableKey& key)
        : m_key(key)
    {
    }

    virtual ~StreamableBase() = default;

    HYP_METHOD()
    HYP_FORCE_INLINE const StreamableKey &GetKey() const
    {
        return m_key;
    }

    HYP_METHOD(Scriptable)
    BoundingBox GetBoundingBox() const;

    HYP_METHOD(Scriptable)
    void OnStreamStart();

    HYP_METHOD(Scriptable)
    void OnLoaded();

    HYP_METHOD(Scriptable)
    void OnRemoved();

protected:
    HYP_METHOD()
    virtual BoundingBox GetBoundingBox_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual void OnStreamStart_Impl()
    {
        // do nothing
    }

    HYP_METHOD()
    virtual void OnLoaded_Impl()
    {
        // do nothing
    }

    HYP_METHOD()
    virtual void OnRemoved_Impl()
    {
        // do nothing
    }

    StreamableKey m_key;
};

} // namespace hyperion

