#ifndef HYPERION_EDITOR_HPP
#define HYPERION_EDITOR_HPP

#include <core/Defines.hpp>

#include <Game.hpp>

namespace hyperion {
class EditorSubsystem;
namespace editor {

class HyperionEditorImpl;

class HYP_API HyperionEditor : public Game
{
public:
    HyperionEditor();
    HyperionEditor(const HyperionEditor& other) = delete;
    HyperionEditor& operator=(const HyperionEditor& other) = delete;
    HyperionEditor(HyperionEditor&& other) noexcept = delete;
    HyperionEditor& operator=(HyperionEditor&& other) noexcept = delete;
    virtual ~HyperionEditor() override;

    virtual void Init() override;
    virtual void Teardown() override;

    virtual void Logic(GameCounter::TickUnit delta) override;
    virtual void OnInputEvent(const SystemEvent& event) override;

protected:
    virtual void PostInit() override;

    HyperionEditorImpl* m_impl;
    Handle<Scene> m_scene; // temp
    RC<EditorSubsystem> m_editor_subsystem;
};
} // namespace editor

using editor::HyperionEditor;

} // namespace hyperion

#endif