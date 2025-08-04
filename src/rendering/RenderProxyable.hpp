/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

namespace hyperion {

class IRenderProxy;

HYP_CLASS(Abstract)
class HYP_API RenderProxyable : public HypObjectBase
{
    HYP_OBJECT_BODY(RenderProxyable);

public:
    virtual ~RenderProxyable() override = default;
    
    /*! \brief Marks this Entity as needing its render proxy to be updated on the next time it is collected. */
    void SetNeedsRenderProxyUpdate();

    virtual void UpdateRenderProxy(IRenderProxy* proxy);

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

protected:
    RenderProxyable();

    virtual void Init() override;

private:
    int m_renderProxyVersion;
};

} // namespace hyperion

