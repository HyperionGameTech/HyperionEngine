/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Name/Name.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class EditorSubsystem;
class EditorActionStack;
class Scene;
class Node;

// an edit mode that swaps the active scene for a private edit scene with its own undo stack
HYP_CLASS(Abstract, Serialize = false)
class EDITOR_API EditorIsolatedEditState : public ObjectBase
{
    HYP_OBJECT_BODY(EditorIsolatedEditState);

public:
    EditorIsolatedEditState();
    ~EditorIsolatedEditState() override;

    void Initialize(EditorSubsystem* subsystem);

    HYP_METHOD()
    virtual bool IsActive() const = 0;

    HYP_METHOD()
    bool CanEnter() const;

    HYP_METHOD()
    bool Exit(bool apply);

    HYP_METHOD()
    virtual bool Apply() = 0;

    HYP_METHOD()
    virtual void Revert() = 0;

    HYP_METHOD()
    bool IsDirty() const;

    HYP_METHOD()
    virtual String GetDisplayName() const = 0;

    HYP_METHOD()
    virtual String GetKindName() const = 0;

    HYP_METHOD()
    const Handle<Scene>& GetEditScene() const;

    HYP_METHOD()
    bool IsEditScene(const Handle<Scene>& scene) const;

    HYP_METHOD()
    EditorActionStack* GetActionStack() const;

    Array<Handle<Scene>> GetIsolatedScenes() const;

    HYP_FIELD()
    ScriptableDelegate<void, bool> OnActiveChanged;

    HYP_FIELD()
    ScriptableDelegate<void, bool> OnDirtyChanged;

protected:
    bool BeginEnter();
    void FinishEnter();

    virtual void ClearTarget() = 0;

    void CreateScenes(Name editSceneName);
    void DestroyScenes();

    void FrameCamera();
    void RestoreCamera();

    void ResetActionStack();
    void SetDirty(bool dirty);

    EditorSubsystem* m_subsystem = nullptr;

    Handle<Scene> m_editScene;
    Handle<Scene> m_stageScene;

    Handle<EditorActionStack> m_actionStack;
    DelegateHandler m_actionStackStateHandler;

    WeakHandle<Scene> m_previousActiveScene;

    Vec3f m_savedCameraTranslation;
    Vec3f m_savedCameraDirection;
    bool m_hasSavedCamera = false;

    bool m_dirty = false;
};

} // namespace Hyperion
