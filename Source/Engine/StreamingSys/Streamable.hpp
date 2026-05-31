/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/name/Name.hpp>

#include <Core/math/BoundingBox.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/HashCode.hpp>

#include <asset/AssetReference.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class ENGINE_API StreamableBase : public ObjectBase
{
    HYP_OBJECT_BODY(StreamableBase);

public:
    StreamableBase() = default;
    virtual ~StreamableBase() = default;

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
};

} // namespace Hyperion
