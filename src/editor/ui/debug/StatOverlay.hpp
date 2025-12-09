/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <core/math/Color.hpp>

#include <core/containers/Array.hpp>

#include <core/Types.hpp>

namespace hyperion {

class World;
class UIText;
class UIListView;

HYP_CLASS()
class HYP_API StatOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(StatOverlay);

    static const Array<Pair<int, Color>> s_fpsColors;

public:
    StatOverlay();
    virtual ~StatOverlay() override;

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    HYP_METHOD()
    virtual int GetPlacement_Impl() const override
    {
        return 1; // Bottom-left corner
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta) override;

    HYP_METHOD()
    virtual bool IsEnabled_Impl() const override
    {
        return true;
    }

private:
    static Color GetFpsColor(int fps);

    Handle<UIText> m_memoryUsageTextElement;
    Handle<UIText> m_fpsTextElement;
    Handle<UIText> m_countersTextElement;

    float m_deltaAccumGame;
    int m_numTicksGame;
};

} // namespace hyperion
