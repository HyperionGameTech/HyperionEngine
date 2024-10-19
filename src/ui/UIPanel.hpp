/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_PANEL_HPP
#define HYPERION_UI_PANEL_HPP

#include <ui/UIObject.hpp>

namespace hyperion {

class UIStage;

#pragma region UIPanel

HYP_CLASS()
class HYP_API UIPanel : public UIObject
{
    HYP_OBJECT_BODY(UIPanel);

protected:
    UIPanel(UIObjectType type);

public:
    UIPanel();
    UIPanel(const UIPanel &other)                   = delete;
    UIPanel &operator=(const UIPanel &other)        = delete;
    UIPanel(UIPanel &&other) noexcept               = delete;
    UIPanel &operator=(UIPanel &&other) noexcept    = delete;
    virtual ~UIPanel() override                     = default;

    virtual bool IsContainer() const override
        { return true; }

    virtual void Init() override;

protected:
    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::ParameterTable GetMaterialParameters() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;
};

#pragma endregion UIPanel

} // namespace hyperion

#endif