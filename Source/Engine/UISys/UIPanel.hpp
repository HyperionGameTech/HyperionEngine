/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <ui/UIObject.hpp>

#include <Core/utilities/EnumFlags.hpp>

namespace Hyperion {

class UIStage;

#pragma region UIPanel

HYP_CLASS()
class ENGINE_API UIPanel : public UIObject
{
    HYP_OBJECT_BODY(UIPanel);

public:
    UIPanel();
    UIPanel(const UIPanel& other) = delete;
    UIPanel& operator=(const UIPanel& other) = delete;
    UIPanel(UIPanel&& other) noexcept = delete;
    UIPanel& operator=(UIPanel&& other) noexcept = delete;
    virtual ~UIPanel() override = default;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsHorizontalScrollEnabled() const
    {
        return bool(m_isScrollEnabled & SA_HORIZONTAL);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsVerticalScrollEnabled() const
    {
        return bool(m_isScrollEnabled & SA_VERTICAL);
    }

    HYP_METHOD()
    void SetIsScrollEnabled(ScrollAxis axis, bool isScrollEnabled);

    virtual bool CanScrollOnAxis(ScrollAxis axis) const override
    {
        const int i = ScrollAxisToIndex(axis);
        if (i == -1)
        {
            return false;
        }

        return (m_isScrollEnabled & axis)
            && GetActualInnerSize()[i] > GetActualSize()[i];
    }

protected:
    virtual void Init() override;

    virtual void OnAttached_Internal(UIObject* parent) override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    virtual void OnScrollOffsetUpdate_Internal(Vec2f delta) override;

    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual MaterialParameters GetMaterialParameters() const override;
    virtual MaterialTextures GetMaterialTextures() const override;

private:
    void SetScrollbarVisible(ScrollAxis axis, bool visible);

    void UpdateScrollbarSizes();
    void UpdateScrollbarSize(ScrollAxis axis);
    void UpdateScrollbarThumbPosition(ScrollAxis axis);

    UIEventHandlerResult HandleScroll(const MouseEvent& eventData);

    EnumFlags<ScrollAxis> m_isScrollEnabled;
    DelegateHandler m_onScrollHandler;

    FixedArray<Vec2i, 2> m_initialDragPosition;
};

#pragma endregion UIPanel

} // namespace Hyperion
