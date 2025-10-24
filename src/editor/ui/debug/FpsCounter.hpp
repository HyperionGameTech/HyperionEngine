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
class HYP_API FpsCounter : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(FpsCounter);

public:
    FpsCounter();
    FpsCounter(World* world);
    virtual ~FpsCounter() override;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    virtual int GetPlacement_Impl() const override
    {
        return 1; // Bottom-left corner
    }

    virtual void Update_Impl(float delta) override;

    virtual Name GetName_Impl() const override
    {
        return NAME("FpsCounter");
    }

    virtual bool IsEnabled_Impl() const override
    {
        return true;
    }

private:
    static Color GetFpsColor(int fps);

    World* m_world;

    Handle<UIText> m_memoryUsageTextElement;
    Handle<UIText> m_fpsTextElement;
    Handle<UIText> m_countersTextElement;

    float m_deltaAccumGame;
    int m_numTicksGame;

    static const Array<Pair<int, Color>> s_fpsColors;
};

HYP_CLASS()
class HYP_API StatOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(StatOverlay);

public:
    StatOverlay();
    virtual ~StatOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    virtual Name GetName_Impl() const override
    {
        return NAME("StatOverlay");
    }

    virtual bool IsEnabled_Impl() const override
    {
        return true;
    }

    virtual void Update_Impl(float delta) override;
};

} // namespace hyperion
