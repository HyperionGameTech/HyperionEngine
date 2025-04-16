#ifndef HYPERION_EDITOR_HPP
#define HYPERION_EDITOR_HPP

#include <core/Defines.hpp>

#include <Game.hpp>

namespace hyperion {
namespace editor {

class HyperionEditorImpl;

class HYP_API HyperionEditor : public Game
{
public:
    HyperionEditor();
    HyperionEditor(const HyperionEditor &other)                 = delete;
    HyperionEditor &operator=(const HyperionEditor &other)      = delete;
    HyperionEditor(HyperionEditor &&other) noexcept             = delete;
    HyperionEditor &operator=(HyperionEditor &&other) noexcept  = delete;
    virtual ~HyperionEditor() override;

    virtual void Init() override;
    virtual void Teardown() override;

    virtual void Logic(GameCounter::TickUnit delta) override;
    virtual void OnInputEvent(const SystemEvent &event) override;

    virtual void OnFrameEnd(FrameBase *frame) override;

protected:
    HyperionEditorImpl  *m_impl;
};
} // namespace editor

using editor::HyperionEditor;

} // namespace hyperion

#endif