#pragma once
#include <core/Defines.hpp>

#include <engine/Game.hpp>

namespace hyperion {
class EditorSubsystem;

class HyperionEditorImpl;

HYP_CLASS(NoScriptBindings)
class HYP_API HyperionEditor : public Game
{
    HYP_OBJECT_BODY(HyperionEditor);

public:
    HyperionEditor();
    HyperionEditor(const HyperionEditor& other) = delete;
    HyperionEditor& operator=(const HyperionEditor& other) = delete;
    HyperionEditor(HyperionEditor&& other) noexcept = delete;
    HyperionEditor& operator=(HyperionEditor&& other) noexcept = delete;
    virtual ~HyperionEditor() override;

    virtual void OnInputEvent(const Event& event) override;

protected:
    HYP_METHOD()
    virtual void OnLaunch_Impl() override;

    HYP_METHOD()
    virtual void OnUpdate_Impl(float delta) override;

    HyperionEditorImpl* m_impl;
    Handle<EditorSubsystem> m_editorSubsystem;
};

} // namespace hyperion
