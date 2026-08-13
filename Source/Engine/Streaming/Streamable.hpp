/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/HashCode.hpp>

#include <Asset/AssetReference.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class ENGINE_API StreamableBase : public ObjectBase
{
    HYP_OBJECT_BODY(StreamableBase);

public:
    StreamableBase() = default;
    virtual ~StreamableBase() = default;

    HYP_METHOD()
    virtual BoundingBox GetBoundingBox() const = 0;

    HYP_METHOD()
    virtual void OnStreamStart()
    {
        // do nothing
    }

    HYP_METHOD()
    virtual void OnLoaded()
    {
        // do nothing
    }

    HYP_METHOD()
    virtual void OnRemoved()
    {
        // do nothing
    }
};

} // namespace Hyperion
