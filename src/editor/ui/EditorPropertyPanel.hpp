/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_PROPERTY_PANEL_HPP
#define HYPERION_EDITOR_PROPERTY_PANEL_HPP

#include <core/object/HypObject.hpp>

#include <ui/UIPanel.hpp>

namespace hyperion {

HYP_CLASS(Abstract)
class HYP_API EditorPropertyPanelBase : public UIPanel
{
    HYP_OBJECT_BODY(EditorPropertyPanelBase);

public:
    EditorPropertyPanelBase();
    EditorPropertyPanelBase(const EditorPropertyPanelBase &other)                   = delete;
    EditorPropertyPanelBase &operator=(const EditorPropertyPanelBase &other)        = delete;
    EditorPropertyPanelBase(EditorPropertyPanelBase &&other) noexcept               = delete;
    EditorPropertyPanelBase &operator=(EditorPropertyPanelBase &&other) noexcept    = delete;
    virtual ~EditorPropertyPanelBase() override;

    HYP_METHOD(Scriptable)
    void Build(const HypData &hyp_data);

    virtual void Init() override;

protected:
    virtual void Build_Impl(const HypData &hyp_data)
        { HYP_PURE_VIRTUAL(); }

    virtual void UpdateSize_Internal(bool update_children) override;

    virtual Material::ParameterTable GetMaterialParameters() const override;

    RC<UIPanel> m_panel;
};

} // namespace hyperion

#endif