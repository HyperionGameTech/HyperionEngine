/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <ui/UIPanel.hpp>

#include <Core/containers/Array.hpp>

namespace Hyperion {

enum class UIDockableContainerFlow : uint32
{
    HORIZONTAL = 0,
    VERTICAL
};

enum class UIDockableItemPosition : uint32
{
    UNDOCKED = 0,
    LEFT,
    RIGHT,
    CENTER,
    TOP,
    BOTTOM,

    MAX
};

#pragma region UIDockableItem

HYP_CLASS()
class HYP_API UIDockableItem : public UIPanel
{
    HYP_OBJECT_BODY(UIDockableItem);

public:
    UIDockableItem();
    UIDockableItem(const UIDockableItem& other) = delete;
    UIDockableItem& operator=(const UIDockableItem& other) = delete;
    UIDockableItem(UIDockableItem&& other) noexcept = delete;
    UIDockableItem& operator=(UIDockableItem&& other) noexcept = delete;
    virtual ~UIDockableItem() override = default;
};

#pragma endregion UIDockableItem

#pragma region UIDockableContainer

HYP_CLASS()
class HYP_API UIDockableContainer : public UIPanel
{
    HYP_OBJECT_BODY(UIDockableContainer);

public:
    UIDockableContainer();
    UIDockableContainer(const UIDockableContainer& other) = delete;
    UIDockableContainer& operator=(const UIDockableContainer& other) = delete;
    UIDockableContainer(UIDockableContainer&& other) noexcept = delete;
    UIDockableContainer& operator=(UIDockableContainer&& other) noexcept = delete;
    virtual ~UIDockableContainer() override = default;

    void AddChildUIObject(const Handle<UIObject>& uiObject, UIDockableItemPosition position);

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

private:
    virtual void Init() override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    void UpdateLayout();

    FixedArray<Handle<UIDockableItem>, uint32(UIDockableItemPosition::MAX)> m_dockableItems;
};

#pragma endregion UIDockableContainer

} // namespace Hyperion
