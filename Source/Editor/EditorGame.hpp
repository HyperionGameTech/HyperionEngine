/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Framework/Game.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {

class EditorSubsystem;
class EditorProject;
class Node;
class Scene;

HYP_CLASS()
class EDITOR_API EditorGame : public Game
{
    HYP_OBJECT_BODY(EditorGame);

public:
    EditorGame();
    virtual ~EditorGame() override;

    HYP_METHOD(Property = "EditorSubsystem", Transient)
    HYP_FORCE_INLINE EditorSubsystem* GetEditorSubsystem() const
    {
        return m_editorSubsystem;
    }

protected:
    virtual void OnLaunch() override;
    virtual void OnUpdate(float delta) override;

    virtual void BeforeShutdown() override;

    HYP_METHOD()
    virtual Handle<World> LoadWorld(Name worldName) override;

private:
    void HandleProjectOpened(const Handle<EditorProject>& project);
    void HandleProjectClosing(const Handle<EditorProject>& project);
    void HandleFocusedNodeChanged(const Handle<Node>& newNode, const Handle<Node>& prevNode, bool shouldSelectInOutline);

    void SetChildAddRemovedHandlers(const Handle<Node>& node);
    void SetRootNodeChangedHandler(const Handle<Scene>& scene);

    EditorSubsystem* m_editorSubsystem;

    DelegateHandler m_onProjectOpened;
    DelegateHandler m_onProjectClosing;

    DelegateHandler m_onFocusedNodeChanged;
    DelegateHandler m_onRootNodeChanged;
    DelegateHandler m_onChildAdded;
    DelegateHandler m_onChildRemoved;
    DelegateHandler m_onActiveSceneChanged;
};

} // namespace Hyperion
