/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_UI_PANEL_HPP
#define HYPERION_V2_UI_PANEL_HPP

#include <ui/UIObject.hpp>

namespace hyperion::v2 {

class UIScene;

#pragma region UIPanel

class HYP_API UIPanel : public UIObject
{
public:
    UIPanel(ID<Entity> entity, UIScene *ui_scene, NodeProxy node_proxy);
    UIPanel(const UIPanel &other)                   = delete;
    UIPanel &operator=(const UIPanel &other)        = delete;
    UIPanel(UIPanel &&other) noexcept               = delete;
    UIPanel &operator=(UIPanel &&other) noexcept    = delete;
    virtual ~UIPanel() override                     = default;

protected:
    virtual Handle<Material> GetMaterial() const override;
};

#pragma endregion UIPanel

} // namespace hyperion::v2

#endif