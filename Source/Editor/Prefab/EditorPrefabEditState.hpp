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

#include <Scripting/ScriptableDelegate.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class EditorSubsystem;
class EditorActionStack;
class Prefab;
class Scene;
class Node;

HYP_CLASS(Serialize = false)
class EDITOR_API EditorPrefabEditState final : public ObjectBase
{
    HYP_OBJECT_BODY(EditorPrefabEditState);

public:
    EditorPrefabEditState();
    ~EditorPrefabEditState() override;

    void Initialize(EditorSubsystem* subsystem);

    HYP_METHOD()
    bool IsActive() const;

    HYP_METHOD()
    bool CanEnter() const;

    HYP_METHOD()
    bool Enter(const Handle<Prefab>& prefab);

    HYP_METHOD()
    void Exit(bool apply);

    HYP_METHOD()
    void Apply();

    HYP_METHOD()
    void Revert();

    HYP_METHOD()
    bool IsDirty() const;

    HYP_METHOD()
    const Handle<Prefab>& GetPrefab() const;

    HYP_METHOD()
    String GetPrefabName() const;

    HYP_METHOD()
    const Handle<Scene>& GetEditScene() const;

    HYP_METHOD()
    bool IsEditScene(const Handle<Scene>& scene) const;

    HYP_METHOD()
    EditorActionStack* GetActionStack() const;

    HYP_METHOD()
    uint32 GetNumLiveInstances() const;

    HYP_METHOD()
    bool IsUpdateInstancesOnApply() const;

    HYP_METHOD()
    void SetUpdateInstancesOnApply(bool updateInstancesOnApply);

    Array<Handle<Scene>> GetIsolatedScenes() const;

    HYP_FIELD()
    ScriptableDelegate<void, bool> OnActiveChanged;

    HYP_FIELD()
    ScriptableDelegate<void, bool> OnDirtyChanged;

private:
    Handle<Node> CloneWorkingRoot() const;
    void SetWorkingRoot(const Handle<Node>& workingRoot);

    void CreateScenes();
    void DestroyScenes();

    void FrameCamera();
    void RestoreCamera();

    void ResetActionStack();
    void SetDirty(bool dirty);

    EditorSubsystem* m_subsystem = nullptr;

    Handle<Prefab> m_prefab;

    Handle<Scene> m_editScene;
    Handle<Scene> m_stageScene;

    Handle<EditorActionStack> m_actionStack;
    DelegateHandler m_actionStackStateHandler;

    WeakHandle<Scene> m_previousActiveScene;

    Vec3f m_savedCameraTranslation;
    Vec3f m_savedCameraDirection;
    bool m_hasSavedCamera = false;

    bool m_dirty = false;
    bool m_updateInstancesOnApply = true;
};

} // namespace Hyperion
